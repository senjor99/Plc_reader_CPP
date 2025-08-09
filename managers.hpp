#pragma once

#include "parser_actions.hpp"
#include "snap7.cpp"
#include "from_pylib.hpp"

class NetManager {
    protected:
        std::map<std::string,std::shared_ptr<DeviceInfo>> devices;
        std::vector<std::string> devices_keys;
        std::shared_ptr<DeviceInfo> device_scope = nullptr;
        
    private:    
        TS7Client Client;
  
    public:
        NetManager() = default;   

        void scan_network(){ 
            devices= return_scan_for_ip();
            devices_keys.clear();
            devices_keys.push_back("--None--");
            for(auto i : devices){
                devices_keys.push_back(i.first);
            }
        }

        std::vector<std::string> get_devices_keys()const{return devices_keys;}
        std::shared_ptr<DeviceInfo> get_device_scope(){return device_scope;}
        std::map<std::string,std::shared_ptr<DeviceInfo>> get_devices_map()const{return devices;}
        std::shared_ptr<DeviceInfo> get_device_from_devices(std::string device_name){
            if(auto it= devices.find(device_name); it != devices.end()){
                return it->second;
            }
        }

        bool set_device_scope(std::string key){
            if(auto it= devices.find(key); it != devices.end()){
                if(it->first == "--None--"){device_scope = nullptr;}
                else {device_scope = it->second;}
                return true;
           }
           else{ return false;}
        }

        void plc_data_retrieve(int db_nr,int size,std::vector<unsigned char>* buffer) {
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

        bool plc_data_send(int db_nr,int size,std::vector<unsigned char> buffer ) {
            if(Client.ConnectTo(device_scope->ip.c_str(),0,1)==0){
                Client.DBWrite(db_nr,0,size,buffer.data());
                return true;
            }
            else{return false;}
            Client.Disconnect();
        }
};

class DatabaseManager {
    protected:
        std::map<std::string, DbInfo> db_map;
        std::vector<std::string> db_keys;
        std::shared_ptr<DB> database = nullptr;
        DbInfo db_scope;
        std::vector<std::string> udt_keys;

    public:
        DatabaseManager()=default;

        // Function for class init
        void lookup_dbs() {
            db_map = return_get_db_list();
            db_keys.clear();
            db_keys.push_back("--None--");

            for(auto i : db_map){
                db_keys.push_back(i.first);
            }
        }
        void create_db() {
            if(db_scope.name == "--None--") {database = nullptr;}
            else{
                ParserState state;
                state.DB_name = db_scope.name;
                pt::file_input<> in("./DBs/"+db_scope.path);        
                pt::parse<complete_datablock,action>(in, state);
                state.db->_set_offset();
                database = state.db;
                for(auto i : state.udt_database){udt_keys.push_back(i.first);}
            }
        }
        // Getter
        std::vector<std::string> get_db_keys()const{ return db_keys;}
        DbInfo get_db_scope()const{return db_scope;}
        std::string get_db_name()const{return db_scope.name;}
        std::string get_db_path()const{return db_scope.path;}
        int get_db_default_number()const{return db_scope.default_number;}        
        int get_db_size()const{return database->offset_max.first;}
        std::vector<std::string> get_udt_keys(){return udt_keys;}
        std::shared_ptr<DB> get_db(){
            if((database==nullptr && db_scope.name != "")|| (database!=nullptr && database->get_name() != db_scope.name))
            {create_db();}

            return database;}

        //Setter
        void set_db_nr(int* nr_in){db_scope.default_number = *nr_in;}

        bool set_db_scope(std::string key){
            if(auto it= db_map.find(key); it != db_map.end()){
                db_scope = it->second;
                return true;
            }
            else{ return false;}

        }

        void set_db_data(const std::vector<unsigned char> buffer){
            database->_set_data(buffer);
        }

        
};


class FilterManager{
    protected:
        std::shared_ptr<DB> db_ptr;
        std::shared_ptr<Filter::BASE_FILTER> filter_ptr;
    public:
        void set_db(std::shared_ptr<DB> db_in){db_ptr = db_in;}
        void set_mode(Filter::filterElem f_el)
        {
            filter_ptr = Filter::Do_Filter(&f_el);
            if(filter_ptr != nullptr)
            {
                filter_ptr->set_filter(db_ptr);
            }
            else
            {
                reset_mode();
            }
         
        }
        void reset_mode()
        {   
            Filter::ResetAll(db_ptr);
        }
      
};

class CommManager {
    protected:
        std::vector<unsigned char> buffer;
        DatabaseManager DataMan;
        NetManager NetMan;
        FilterManager FilMan;

    public:
        CommManager()=default;
            
        std::vector<std::string> get_devices_keys()const{return NetMan.get_devices_keys();}
        std::vector<std::string> get_databases_keys()const{return DataMan.get_db_keys();}
        std::shared_ptr<DB> get_database(){return DataMan.get_db();}
        std::map<std::string,std::shared_ptr<DeviceInfo>> get_devices_map()const{return NetMan.get_devices_map();}
        std::shared_ptr<DeviceInfo> get_device_scope(){return NetMan.get_device_scope();}
        DbInfo get_database_scope(){return DataMan.get_db_scope();}
        std::vector<std::string> get_udt_keys(){return DataMan.get_udt_keys();}
        void get_plc_data(){
            NetMan.plc_data_retrieve(DataMan.get_db_default_number(),DataMan.get_db_size()+1,&buffer);
            DataMan.set_db_data(buffer);
        }

        void set_plc_data(){NetMan.plc_data_retrieve(DataMan.get_db_default_number(),DataMan.get_db_size(),&buffer);}
        void set_device_scope(std::string& el_name_in){NetMan.set_device_scope(el_name_in);}
        void set_filter_mode(Filter::filterElem f_el){FilMan.set_mode(f_el);}
        void reset_filter_mode(){FilMan.reset_mode();}
        void set_database_scope(std::string& el_name_in)
        {
            DataMan.set_db_scope(el_name_in);
            FilMan.set_db(DataMan.get_db());
        }
        
        void create_database(){DataMan.create_db();}

        void update_elem(){ NetMan.scan_network(); DataMan.lookup_dbs();}

        void set_db_nr(int* val){DataMan.set_db_nr(val);}        
};

