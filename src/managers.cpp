#include <managers.hpp>
#include <classes.hpp>
#include <hw_interface.hpp>
#include <action.hpp>

// class NetManager 
/*

*/
std::vector<std::string> NetManager::get_devices_keys()const{return devices_keys;}

std::shared_ptr<DeviceProfInfo> NetManager::get_device_scope(){return device_scope;}

std::map<std::string,std::shared_ptr<DeviceProfInfo>> NetManager::get_devices_map()const{return devices;}

std::string NetManager::get_subnet(){return subnet;}

void NetManager::scan_network(){ 
    std::cout<<"Manager Getting Devices"<<"\n";
    //devices= ethernet::get_devices(subnet);
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
    else return nullptr;
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

void DatabaseManager::create_db() 
{
    if(db_scope.name == "" ) {database = nullptr;}
    else{
        
        ParserState state;
        state.DB_name = db_scope.name;

        pt::file_input<> in(db_scope.path);        
        pt::parse<complete_datablock,action>(in, state);


        if(state.db == nullptr) std::cerr<<"DB not created";
        state.db->_set_offset();
        database = state.db;
    }
}

std::string DatabaseManager::get_db_name()const{return db_scope.name;}

std::string DatabaseManager::get_db_path()const{return db_scope.path;}

int DatabaseManager::get_db_default_number()const{return db_scope.default_number;} 
      
int DatabaseManager::get_db_size()const{return database->get_max_offset().first;}

std::shared_ptr<DB> DatabaseManager::get_db(){return database;}

void DatabaseManager::set_db_nr(int* nr_in){db_scope.default_number = *nr_in;}

void DatabaseManager::set_db_scope(DbInfo key){db_scope = key;create_db();}

void DatabaseManager::set_db_data(const std::vector<unsigned char> buffer){database->_set_data(buffer);}


// class DatabaseManager 

// class FilterManager 
/*

*/
void FilterManager::set_mode(Filter::filterElem f_el)
{

    filter_ptr = Filter::Do_Filter(&f_el);
    if(filter_ptr != nullptr)
    {
        filter_ptr->set_filter(db_ptr);
    }
}

void FilterManager::set_dbPtr(std::shared_ptr<DB> ptr){db_ptr = ptr;};

// class FilterManager 

// class CommManager 
/*

*/

CommManager::CommManager()=default;
CommManager::~CommManager()=default; 

std::vector<std::string> CommManager::get_devices_keys()const{return NetMan.get_devices_keys();}

std::map<std::string,std::shared_ptr<DeviceProfInfo>> CommManager::get_devices_map()const{return NetMan.get_devices_map();}

std::shared_ptr<DeviceProfInfo> CommManager::get_device_scope(){return NetMan.get_device_scope();}

void CommManager::get_plc_data(){
    NetMan.plc_data_retrieve(DataMan.get_db_default_number(),DataMan.get_db_size()+1,&buffer);
    DataMan.set_db_data(buffer);
}

_folder_ CommManager::get_directory(){return folders::get_instances();};

void CommManager::set_plc_data(){NetMan.plc_data_retrieve(DataMan.get_db_default_number(),DataMan.get_db_size(),&buffer);}

void CommManager::set_device_scope(std::string& el_name_in){NetMan.set_device_scope(el_name_in);}

void CommManager::set_filter_mode(Filter::filterElem f_el)
{
    FilMan.set_dbPtr(DataMan.get_db());
    FilMan.set_mode(f_el);
}

void CommManager::add_new_db(){}

void CommManager::refresh_device(){NetMan.scan_network();}

void CommManager::set_db_nr(int* val){DataMan.set_db_nr(val);}        

void CommManager::add_directory(){};