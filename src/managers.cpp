#include <managers.hpp>
#include <classes.hpp>
#include <hw_interface.hpp>
#include <action.hpp>
#include <thread>
#include <profi_DCP.hpp>

/*-------------------------------------------------------*/
/*------------------- Network Manager -------------------*/ 
/*-------------------------------------------------------*/


/// Constructor that initializes the Profinet client for network operations.
NetManager::NetManager()
    :network(profinet::PcapClient()) {}

/// Launches a network scan to identify all available devices.
///  Take a look into profi_DCP.cpp for more information
void NetManager::scan_network() { network.identifyAll(); }

/// Returns a map of available network cards detected by the Profinet client.
const std::map<std::string,std::string> NetManager::get_netCards(){ return network.get_cards(); }

/// Returns a list of stored device keys for quick access.
const std::vector<profinet::DCP_Device>* NetManager::get_devices() { return network.get_devices(); }

/// Returns the ip setted from the gui into the manager, can be a std::nullopt.
const std::optional<std::string> NetManager::get_ip() { return ip_selected; }

///  Connects to the active device and reads data from the specified PLC datablock into a buffer.
void NetManager::plc_data_retrieve(int db_nr,int size,std::vector<unsigned char>* buffer) 
{
    buffer->resize(size);
    if (!buffer) {
        std::cerr << "ERRORE: buffer è null!\n";
    }
    if (Client.ConnectTo(ip_selected.value().c_str(), 0, 1) == 0){
        int res= Client.DBRead(db_nr,0,size,buffer->data());
        Client.Disconnect();
    }
    else{ 
        std::cerr<<"Error connecting to client\n";
        return ;}
}

/// Connects to the active device and writes data to the specified PLC datablock.
bool NetManager::plc_data_send(int db_nr,int size,std::vector<unsigned char> buffer ) 
{
    if(Client.ConnectTo(ip_selected.value().c_str(),0,1)==0){
        Client.DBWrite(db_nr,0,size,buffer.data());
        return true;
    }
    else{return false;}
    Client.Disconnect();
}

/// Selects which network card to use for communication.
void NetManager::set_netCard(std::string card){network.set_card(card);};


void NetManager::set_ip(std::string ip) { ip_selected = ip;}
/*-------------------------------------------------------------------------------------*/


/*-------------------------------------------------------*/
/*------------------ Database Manager -------------------*/ 
/*-------------------------------------------------------*/

/// Parses and builds a new database object from the selected file path using the grammar parser.
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

/// Returns the name of the currently loaded database.
std::string DatabaseManager::get_db_name()const{return db_scope.name;}

/// Returns the file path of the current database.
std::string DatabaseManager::get_db_path()const{return db_scope.path;}

///  Returns the default datablock number associated with the database.
int DatabaseManager::get_db_default_number()const{return db_scope.default_number;} 
      
/// Returns the calculated maximum size of the current database. =^.^=
int DatabaseManager::get_db_size()const{return database->get_max_offset().first;}

/// Returns the current database object.
std::shared_ptr<DB> DatabaseManager::get_db(){return database;}

/// Updates the default datablock number.
/// It is necessary to be setted to perform readDB with snap7 lib 
void DatabaseManager::set_db_nr(int* nr_in){db_scope.default_number = *nr_in;}

/// Sets the database scope (metadata) and triggers creation of the DB structure.
void DatabaseManager::set_db_scope(DbInfo key){db_scope = key;create_db();}

/// Loads raw PLC data into the database object for interpretation.
void DatabaseManager::set_db_data(const std::vector<unsigned char> buffer){database->_set_data(buffer);}

/*-------------------------------------------------------------------------------------*/


/*-------------------------------------------------------*/
/*------------------- Filter Manager --------------------*/ 
/*-------------------------------------------------------*/

/// Applies a filter operation to the current database using the provided filter element.
/// Possibilities of filter are Value, name or both togheter, more filter will be implemented in future
void FilterManager::set_mode(Filter::filterElem f_el)
{

    filter_ptr = Filter::Do_Filter(&f_el);
    if(filter_ptr != nullptr)
    {
        filter_ptr->set_filter(db_ptr);
    }
}

/// Sets the database pointer to be used as the target for filtering operations.
void FilterManager::set_dbPtr(std::shared_ptr<DB> ptr){db_ptr = ptr;};

/*-------------------------------------------------------------------------------------*/


/*-------------------------------------------------------*/
/*------------------- Common Manager --------------------*/ 
/*-------------------------------------------------------*/

/// Default class Constructor.
CommManager::CommManager()=default;

/// Default class destructor.
CommManager::~CommManager()=default; 

/// Reads PLC data through NetManager and updates the DatabaseManager with the new buffer.
void CommManager::get_plc_data(){
    NetMan.plc_data_retrieve(DataMan.get_db_default_number(),DataMan.get_db_size()+1,&buffer);
    DataMan.set_db_data(buffer);
}

/// Retrieves the directory object from the folder manager.
_folder_ CommManager::get_directory(){return folders::get_instances();};

/// Writes the current buffer to the PLC via NetManager.
void CommManager::set_plc_data(){NetMan.plc_data_retrieve(DataMan.get_db_default_number(),DataMan.get_db_size(),&buffer);}

/// Applies a filtering mode to the database through the FilterManager.
void CommManager::set_filter_mode(Filter::filterElem f_el)
{
    FilMan.set_dbPtr(DataMan.get_db());
    FilMan.set_mode(f_el);
}