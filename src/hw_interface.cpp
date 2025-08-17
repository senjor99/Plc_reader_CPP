#include <hw_interface.hpp>


std::vector<std::string> folders::extensions = {".db",".udt"};
std::string folders::directory = "./instances";

std::map<std::string, std::shared_ptr<DeviceProfInfo>> ethernet::get_devices(std::string& sub )
{
    std::cout<<"starting scan1"<<"\n";
    std::map<std::string,std::shared_ptr<DeviceProfInfo>> res;
    std::cout<<"starting scan2"<<"\n";
    std::shared_ptr<DeviceProfInfo> DevInfo;
    std::cout<<"starting scan3"<<"\n";
    TS7CpuInfo cp_res ;
    std::cout<<"starting scan4"<<"\n";

    for(int i = 0; i<255;i++)
    {
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




void folders::_erase_extensions(std::vector<std::string>& phs)
{
    for(auto& ph : phs)
    {
        ph =ph.substr(0, ph.find(".db"));
        ph =ph.substr(0, ph.find(".udt"));
    }
} 

std::vector<std::string> folders::_erase_base(std::vector<std::string> phs)
{
    std::string base = std::filesystem::current_path().string();
    for(auto ph : phs)
    {
        ph=ph.substr(0, ph.find("base"));
    }
    return phs;
} 

std::map<std::string, DbInfo> folders::get_dbs()
{
    std::string directory = "./instances";
    std::map<std::string, DbInfo> res;
    std::string tmp_path;
    std::vector<std::string> tmp_c_paths;
    std::vector<std::string> names;

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
    _erase_extensions(tmp_c_paths);
    names = _erase_base(tmp_c_paths);
    for(int i = 0 ; names.size();i ++)
    {
        DbInfo newinfo;
        newinfo.default_number = 0;
        newinfo.name = names[i];
        newinfo.path = tmp_c_paths[i];
        res[names[i]] = newinfo; 
    }
    return res;
}

