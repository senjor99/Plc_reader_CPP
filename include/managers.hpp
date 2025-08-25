#pragma once

#include "datatype.hpp"
#include <memory>
#include <classes.hpp>
#include <profi_DCP.hpp>

class NetManager {
    private:    
        
        TS7Client Client;
        profinet::PcapClient network;
        std::optional<std::string> ip_selected = std::nullopt;
        std::vector<profinet::DCP_Device> devices;

    public:
        NetManager();   

        void scan_network();
        
        const std::map<std::string,std::string> get_netCards();
        std::vector<profinet::DCP_Device>* get_devices();
        const std::optional<std::string> get_ip();

        void plc_data_retrieve(int db_nr,int size,std::vector<unsigned char>* buffer);
        bool plc_data_send(int db_nr,int size,std::vector<unsigned char> buffer ) ;
        void set_netCard(std::string card);
        void set_ip(std::string ip);
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
        void set_mode(Filter::filterElem f_el);
        void set_dbPtr(std::shared_ptr<DB> ptr);
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

        void get_plc_data();
        _folder_ get_directory();

        void set_plc_data();
        void set_filter_mode(Filter::filterElem f_el);

};

