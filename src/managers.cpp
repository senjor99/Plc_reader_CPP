#include <managers.hpp>
#include <classes.hpp>
#include <hw_interface.hpp>
#include <parser.hpp>

// class NetManager 
/*

*/
std::vector<std::string> NetManager::get_devices_keys()const{return devices_keys;}

std::shared_ptr<DeviceProfInfo> NetManager::get_device_scope(){return device_scope;}

std::map<std::string,std::shared_ptr<DeviceProfInfo>> NetManager::get_devices_map()const{return devices;}


void NetManager::scan_network(){ 
    std::cout<<"Manager Getting Devices"<<"\n";
    devices= ethernet::get_devices(subnet);
    devices_keys.clear();
    devices_keys.push_back("--None--");
    for(auto i : devices){
        devices_keys.push_back(i.first);
    }
}

std::shared_ptr<DeviceProfInfo> NetManager::get_device_from_devices(std::string device_name){
    if(auto it= devices.find(device_name); it != devices.end()){
        return it->second;
    }
}

void NetManager::set_subnet(std::string sub){subnet = sub;}

bool NetManager::set_device_scope(std::string key){
    if(auto it= devices.find(key); it != devices.end()){
        if(it->first == "--None--"){device_scope = nullptr;}
        else {device_scope = it->second;}
        return true;
    }
    else{ return false;}
}

void NetManager::plc_data_retrieve(int db_nr,int size,std::vector<unsigned char>* buffer) {
    buffer->resize(size);
    if (!buffer) {
        std::cerr << "ERRORE: buffer è null!\n";
    }
    if (Client.ConnectTo(device_scope->ip.c_str(), 0, 1) == 0){
        int res= Client.DBRead(db_nr,0,size,buffer->data());
        Client.Disconnect();
    }
    else{ 
        std::cerr<<"Error connecting to client\n";
        return ;}
}

bool NetManager::plc_data_send(int db_nr,int size,std::vector<unsigned char> buffer ) {
    if(Client.ConnectTo(device_scope->ip.c_str(),0,1)==0){
        Client.DBWrite(db_nr,0,size,buffer.data());
        return true;
    }
    else{return false;}
    Client.Disconnect();
}

// class NetManager 

// class DatabaseManager 
/*

*/

void DatabaseManager::lookup_dbs() 
{
    db_map = folders::get_dbs();
    db_keys.clear();
    db_keys.push_back("--None--");

    for(auto i : db_map){
        db_keys.push_back(i.first);
    }
}
void DatabaseManager::create_db() 
{
    if(db_scope.name == "--None--") {database = nullptr;}
    else{
        ParserState state;
        state.DB_name = db_scope.name;
        pt::file_input<> in("./DBs/"+db_scope.path);        
        auto var = pt::parse<complete_datablock,action>(in, state);
        state.db->_set_offset();
        database = state.db;
        for(auto i : state.udt_database){udt_keys.push_back(i.first);}
    }
}

std::vector<std::string> DatabaseManager::get_db_keys()const{ return db_keys;}

DbInfo DatabaseManager::get_db_scope()const{return db_scope;}

std::string DatabaseManager::get_db_name()const{return db_scope.name;}

std::string DatabaseManager::get_db_path()const{return db_scope.path;}

int DatabaseManager::get_db_default_number()const{return db_scope.default_number;} 
      
int DatabaseManager::get_db_size()const{return database->get_max_offset().first;}

std::vector<std::string> DatabaseManager::get_udt_keys(){return udt_keys;}

std::shared_ptr<DB> DatabaseManager::get_db()
{
    if((database==nullptr && db_scope.name != "")|| (database!=nullptr && database->get_name() != db_scope.name))
    {create_db();}

    return database;
}

void DatabaseManager::set_db_nr(int* nr_in){db_scope.default_number = *nr_in;}

bool DatabaseManager::set_db_scope(std::string key){
    if(auto it= db_map.find(key); it != db_map.end()){
        db_scope = it->second;
        return true;
    }
    else{ return false;}

}

void DatabaseManager::set_db_data(const std::vector<unsigned char> buffer){
    database->_set_data(buffer);
}

// class DatabaseManager 

// class FilterManager 
/*

*/

void FilterManager::set_db(std::shared_ptr<DB> db_in){db_ptr = db_in;}

void FilterManager::set_mode(Filter::filterElem f_el)
{
    filter_ptr = Filter::Do_Filter(&f_el);
    if(filter_ptr != nullptr)
    {
        filter_ptr->set_filter(db_ptr);
    }
    else
    {
        FilterManager::reset_mode();
    }
    
}
void FilterManager::reset_mode()
{   
    Filter::ResetAll(db_ptr);
}
// class FilterManager 

// class CommManager 
/*

*/

CommManager::CommManager()=default;
CommManager::~CommManager()=default; 

std::vector<std::string> CommManager::get_devices_keys()const{return NetMan.get_devices_keys();}

std::vector<std::string> CommManager::get_databases_keys()const{return DataMan.get_db_keys();}

std::shared_ptr<DB> CommManager::get_database(){return DataMan.get_db();}

std::map<std::string,std::shared_ptr<DeviceProfInfo>> CommManager::get_devices_map()const{return NetMan.get_devices_map();}

std::shared_ptr<DeviceProfInfo> CommManager::get_device_scope(){return NetMan.get_device_scope();}

DbInfo CommManager::get_database_scope(){return DataMan.get_db_scope();}

std::vector<std::string> CommManager::get_udt_keys(){return DataMan.get_udt_keys();}

void CommManager::get_plc_data(){
    NetMan.plc_data_retrieve(DataMan.get_db_default_number(),DataMan.get_db_size()+1,&buffer);
    DataMan.set_db_data(buffer);
}

void CommManager::set_plc_data(){NetMan.plc_data_retrieve(DataMan.get_db_default_number(),DataMan.get_db_size(),&buffer);}

void CommManager::set_device_scope(std::string& el_name_in){NetMan.set_device_scope(el_name_in);}

void CommManager::set_filter_mode(Filter::filterElem f_el){FilMan.set_mode(f_el);}

void CommManager::reset_filter_mode(){FilMan.reset_mode();}

void CommManager::set_database_scope(std::string& el_name_in)
{
    DataMan.set_db_scope(el_name_in);
    FilMan.set_db(DataMan.get_db());
}

void CommManager::create_database(){DataMan.create_db();}

void CommManager::update_elem(){std::cout<<"Manager called"<<"\n";NetMan.scan_network(); DataMan.lookup_dbs();}

void CommManager::set_db_nr(int* val){DataMan.set_db_nr(val);}        
