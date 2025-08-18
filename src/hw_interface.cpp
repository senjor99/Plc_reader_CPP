#include <hw_interface.hpp>


std::vector<std::string> folders::extensions = {".db",".udt"};
std::string folders::directory = "./instances";

std::map<std::string, std::shared_ptr<DeviceProfInfo>> ethernet::get_devices(std::string& sub)
{
    std::map<std::string,std::shared_ptr<DeviceProfInfo>> res;
    std::shared_ptr<DeviceProfInfo> DevInfo;
    TS7CpuInfo cp_res ;

    for(int i = 0; i<255;i++)
    {
        std::string subnet = sub != "" ? sub  : "10.94.248";
        std::string ip = sub+std::to_string(i); 
        
        if(_ping(ip)) if(ethernet::_GetCpuInfo(ip,cp_res))
        {
                DevInfo = DeviceProfInfo::ptr_from_S7CPUinfo(cp_res,ip);
                res[DevInfo->profiname] = DevInfo;
        }
    }
    return res;
}

bool ethernet::_GetCpuInfo(std::string& ip,TS7CpuInfo& name)
{
    TS7Client cli ;
    if(cli.ConnectTo(ip.c_str(),0,1)==0) {
        
        cli.GetCpuInfo(&name);
        cli.Disconnect();
        return true;
    }

    return false;
}
bool ethernet::_ping(std::string& ip)
{
    std::cout<<"running command1"<<"\n";
    std::string cmd = "ping -n 1 -w 100 " + ip ;//+ " >nul 2>&1";
    std::cout<<"running command2"<<"\n";
    return system(cmd.c_str()) == 0;
}


void folders::_erase_extensions(std::string& ph)
{
    ph =ph.substr(0, ph.find(".db"));
    ph =ph.substr(0, ph.find(".udt"));
} 

bool folders::isNumber(char c) {
    return std::isdigit(static_cast<unsigned char>(c));
}

std::pair<int,std::string> folders::_erase_def_nr(std::string ph)
{
    _erase_extensions(ph);
    std::string name_res;
    std::string _number;

    ph = ph.substr(0,ph.find("]"));

    for(auto ch : ph)
    {
        if(folders::isNumber(ch)) _number.push_back(ch);
    }
    ph= ph.substr(0,ph.find("["));
    
    std::cout<<_number<<" "<<ph<<"\n";
    return {std::stoi(_number),ph};

};

std::map<std::string, DbInfo> folders::get_dbs()
{
    std::string directory = "./instances";
    std::map<std::string, DbInfo> res;
    std::string tmp_path;
    std::vector<std::string> tmp_c_paths;
    std::string base = std::filesystem::current_path().string();

    if(!fs::exists(directory) || !fs::is_directory(directory))
    {
        std::cerr << "Missing direcory or wrong path, trying creating new";
        fs::create_directory(directory);
    }
    
    for(const auto& i : fs::directory_iterator(directory))
    {
        tmp_path = i.path().filename().string();
        tmp_c_paths.push_back(tmp_path);
    }
   
    


    for(auto& i : tmp_c_paths)
    {
        DbInfo newinfo;
        std::pair<int,std::string> nr_name =folders::_erase_def_nr(i);
        newinfo.default_number = nr_name.first;
        newinfo.name = nr_name.second;
        newinfo.path = base+"\\instances\\"+i;
        res[nr_name.second] = newinfo; 
    }
    return res;
}

