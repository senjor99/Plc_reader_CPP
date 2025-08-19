#pragma once

#include "datatype.hpp"
#include <memory>
#include <classes.hpp>

class NetManager {
    protected:
        std::map<std::string,std::shared_ptr<DeviceProfInfo>> devices;
        std::vector<std::string> devices_keys;
        std::shared_ptr<DeviceProfInfo> device_scope = nullptr;
        std::string subnet;

    private:    
        TS7Client Client;
  
    public:
        NetManager() = default;   

        void scan_network();

        std::vector<std::string> get_devices_keys()const;
        std::shared_ptr<DeviceProfInfo> get_device_scope();
        std::map<std::string,std::shared_ptr<DeviceProfInfo>> get_devices_map()const;
        std::shared_ptr<DeviceProfInfo> get_device_from_devices(std::string device_name);
        std::string get_subnet();
        
        bool set_device_scope(std::string key);
        void set_subnet(std::string sub);
        void plc_data_retrieve(int db_nr,int size,std::vector<unsigned char>* buffer);
        bool plc_data_send(int db_nr,int size,std::vector<unsigned char> buffer ) ;
};

class DatabaseManager {
    protected:
        std::shared_ptr<DB> database = nullptr;
        DbInfo db_scope;

    public:
        DatabaseManager()=default;

        // Function for class init
        void create_db();
        
        // Getter
        std::string get_db_name()const;
        std::string get_db_path()const;
        int get_db_default_number()const;     
        int get_db_size()const;
        std::shared_ptr<DB> get_db();

        //Setter
        void set_db_nr(int* nr_in);
        void set_db_scope(DbInfo key);
        void set_db_data(const std::vector<unsigned char> buffer);
};


class FilterManager{
    protected:
        std::shared_ptr<DB> db_ptr;
        std::shared_ptr<Filter::BASE_FILTER> filter_ptr;
    public:
        void set_db(std::shared_ptr<DB> db_in);
        void set_mode(Filter::filterElem f_el);
        void reset_mode();
      
};

class CommManager
{
    public:
        std::vector<unsigned char> buffer;
        DatabaseManager DataMan;
        NetManager NetMan;
        FilterManager FilMan;

        CommManager();
        ~CommManager();  

        std::vector<std::string> get_devices_keys()const;
        std::map<std::string,std::shared_ptr<DeviceProfInfo>> get_devices_map()const;
        std::shared_ptr<DeviceProfInfo> get_device_scope();
        DbInfo get_database_scope();
        void get_plc_data();
        _folder_ get_directory();

        void set_plc_data();
        void set_device_scope(std::string& el_name_in);
        void set_filter_mode(Filter::filterElem f_el);
        void refresh_device();
        void set_db_nr(int* val);    
        void add_new_db();    
        void add_directory();    

};

