import json
import os
import re
import snap7
from snap7.util.getters import *
from snap7.util.setters import *
from pnio_dcp import*
from typing import List
import netifaces
from pyparsing import ParseResults

class settings():
    '''
    Utility class for setting data into files or rectuit information from files
    '''
    def __init__(self):
        with open("py_lib/settings.json","r") as f:
            self.directories = json.load(f)
        self.base = self.directories["base_dir"]
    @classmethod
    def get_db_file(cls,name:str)->str:
        self = cls()
        path=os.path.join(self.base,self.directories["DBs_folder"],name)
        with open(path, encoding="utf-8-sig") as f:
            DB = f.read()
            return DB
    @classmethod
    def get_db_list(cls)->dict:
        self = cls()
        path= os.path.join(self.base,self.directories["DBs_folder"])
        dbs = os.listdir(path)
        db_list={}
        for db in dbs:
            match = re.search(r'\[(\d+)\]', db)
            if match:
                nr = str(match.group(1))
                name= re.sub(".db", "", db)
                name= re.sub(r'\[\d+\]', "", name)

                db_list[name] = {
                    "name":name,
                    "path":db,
                    "default_number":nr
                    }
        keys = list(db_list.keys())
        keys.sort()
        return db_list,keys
    
#=========================================================================
def network_self_ip():
    """
    Returns the static ip of the network device installed on the machine
    """
    for iface in netifaces.interfaces():
        addrs = netifaces.ifaddresses(iface)
        if netifaces.AF_INET in addrs:
            for addr in addrs[netifaces.AF_INET]:
                ip = addr['addr']
                if ip.startswith("192.168.") or ip.startswith("10."):
                    return ip
    raise RuntimeError("No valid Profinet IP found.")

#=========================================================================
def scan_for_ip()->dict:
    """
    Scan the network looking for S7 devices

    Return a python dictionary with a list of key to access the various device information
    use ["IP"] for rectuit the ip address
    use ["Raw_ProfiName"] for recruit the full profinet name with the interface
    use ["ProfiName"] for recruit the name splitted by the interface name
    """
    print("obtaining card ip")
    ip = network_self_ip()
    print("scanning..")
    dcp = pnio_dcp.DCP(ip)
    obj_list:List[Device] =dcp.identify_all()
    found_devices = {}
    for i in obj_list:
        if "S7" in i.family and "eth" in i.name_of_station : 
            name,x = i.name_of_station.split(".",1)
            found_devices[name] = {"IP":i.IP,"Raw_ProfiName":i.name_of_station,"ProfiName":name}
    keys = list(found_devices.keys())
    keys.sort()
    return found_devices,keys

#=========================================================================
def get_db_list_()->list:
    """
    Returns a python dictionary of db list in the folder "DBs" with a list of keys
    
    use ["name"] for the name splitted by the db number
    use ["full_name"] for the full name 
    use ["number] for the default db number

    DBs position can be modified into the settings.json
    """

    return settings.get_db_list()

#=========================================================================
def from_py_to_cpp_convert(p: ParseResults):
    """
    Just convert pyparser dictionary into a python dictionary
    """
    if isinstance(p, ParseResults):
        if p.keys():
            return {k: from_py_to_cpp_convert(p[k]) for k in p.keys()}
        else:
            return [from_py_to_cpp_convert(i) for i in p]
    else:
        return p

#=========================================================================    
def parse_db(db_name):
    """
    Convert the normal .db structure of a Siemens Database into a pyparser dictionary
    Structred as:
    {
        udt_field:
            udt:[
                "name"
                "type"
                "array:length":[    #Optional
                    "start_array"
                    "end_array"
                ]
            ]
        "struct_field":
            [
                "name"
                "type"
                "array:length":[    #Optional
                    "start_array"
                    "end_array"
                ]
            ]
    }

    The udt field can be used to construct a list of shared udt, without constructing it everytime
    in the struct field name of udt will be the one assigned, in the type you'll find the parent udt name 
    parser_map = map_struct.parseString(settings.get_db_file(db_name))
    return from_py_to_cpp_convert(parser_map)
    """

#=========================================================================
def get_data_from_plc(ip_to_connect:str,db_nr:int,max_offset:int)->bytearray|bool:
    """
    Connect to a plc via Snap7 Library, it is necessary to specify the ip, number of the Database and size as the
    max offset reached by the Database.
    It return a Database that can be simply be handled by the get_data_to_bytearray function down there

    """
    client = snap7.client.Client()
    client.connect(ip_to_connect,0,0)
    if client.get_connected():
        pass
    else:
        return False
    data = client.db_read(int(db_nr),0,int(max_offset))        
    return data

#=========================================================================
def set_data_to_plc(ip_to_connect,db_nr,data_to_write)->bool:
    """
    Connect to a plc via Snap7 Library, it is necessary to specify the ip, number of the Database and size as the
    max offset reached by the Database and the data to write into the Database
    It return True if the connection gone fine and False in case of failure to connection.
    Bytearray che be set simply by using the set_data_to_bytearray function down there

    """
    client = snap7.client.Client()
    client.connect(str(ip_to_connect),0,0)
    if client.get_connected():
        pass
    else:
        return False
    client.db_write(int(db_nr),0,data_to_write)
    return True

#=========================================================================
def set_data_to_bytearray(type_:str,old_data:bytearray,offset:tuple,data:str|int)->bytearray:
    match type_.lower():
        case "bool":data_result= set_bool(old_data,offset[0],offset[1],bool(data))
        case "byte":data_result= set_byte(old_data,offset[0],int(data))
        case "date":return print("NOT IMPLEMENTED YET") # set_date(old_data,offset[0],date(data))
        case "dint":data_result= set_dint(old_data,offset[0],int(data))
        case "dword":data_result= set_dword(old_data,offset[0],int(data))
        case "fstring":data_result= set_fstring(old_data,offset[0],data)
        case "int":data_result= set_int(old_data,offset[0],data)
        case "lreal":data_result= set_lreal(old_data,offset[0],float(data))
        case "real":data_result= set_real(old_data,offset[0],int(data))
        case "sint":data_result= set_sint(old_data,offset[0],int(data))
        case "string":data_result= set_string(old_data,offset[0],data)
        case "time":data_result= set_time(old_data,offset[0],data)
        case "udint":data_result= set_udint(old_data,offset[0],int(data))
        case "uint:":data_result= set_uint(old_data,offset[0],int(data))
        case "usint":data_result= set_usint(old_data,offset[0],int(data))
        case "word":data_result= set_word(old_data,offset[0],int(data))
        case 'char':data_result= set_char(old_data,offset[0], data)
        case _: return print("Error in datatype")
    return data_result

#=========================================================================
def get_data_from_bytearray(type_:str,offset:tuple,_data:bytearray)->str|int:
    match type_.lower():
        case "bool":result= get_bool(_data,offset[0],offset[1])
        case "byte":result= get_byte(_data,offset[0])
        case "date":result= get_date(_data,offset[0])
        case "dint":result= get_dint(_data,offset[0])
        case "dword":result= get_dword(_data,offset[0])
        case "fstring":result= get_fstring(_data,offset[0])
        case "int":result= get_int(_data,offset[0])
        case "lreal":result= get_lreal(_data,offset[0])
        case "real":result= get_real(_data,offset[0])
        case "sint":result= get_sint(_data,offset[0])
        case "string":result= get_string(_data,offset[0])
        case "time":result= get_time(_data,offset[0])
        case "udint":result= get_udint(_data,offset[0])
        case "uint:":result= get_uint(_data,offset[0])
        case "usint":result= get_usint(_data,offset[0])
        case "word":result= get_word(_data,offset[0])
        case 'char':result= get_char(_data,offset[0])
        case _: return print("Error in datatype")
    return result
