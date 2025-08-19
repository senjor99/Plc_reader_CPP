#include <hw_interface.hpp>


std::vector<std::string> folders::extensions = {".db",".udt"};
std::string folders::directory = "./root";

std::map<std::string, std::shared_ptr<DeviceProfInfo>> ethernet::get_devices(std::string& sub)
{
    std::map<std::string,std::shared_ptr<DeviceProfInfo>> res;
    std::shared_ptr<DeviceProfInfo> DevInfo;
    TS7CpuInfo cp_res ;

    for(int i = 0; i<255;i++)
    {
        std::string subnet = sub != "" ? sub  : "10.94.248";
        std::string ip = sub+std::to_string(i); 
        
        if(_ping(ip)) 
            if(ethernet::_GetCpuInfo(ip,cp_res))
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
    //    cli.GetCpuInfo(&name);
        cli.Disconnect();
        return true;
    }
    return false;
}

bool ethernet::_ping(std::string& ip)
{
    std::string cmd = "ping -n 1 -w 100 " + ip ;// +" >nul 2>&1";

    return system(cmd.c_str()) == 0;
}

void walk_dir(_folder_& dir, const std::string& path, std::string& f_path)
{
    for (const auto& i : fs::directory_iterator(path))
    {
        if (i.is_directory())
        {
            _folder_ subfolder(i.path().filename().string());
            f_path += "/"+subfolder.name;
            subfolder.path=f_path;
            walk_dir(subfolder, i.path().string(),f_path);   
            dir.elements.push_back(std::move(subfolder));
        
        }
        else
        {
            _file_ new_file(i.path().filename().string());
            new_file.path=f_path+"/"+new_file.name;
            dir.elements.push_back(std::move(new_file));
        }
    }
}

_folder_ folders::get_instances()
{
    auto base =std::filesystem::current_path().string();

    std::string directory = base+"/root";
    std::string full_path = directory;

    if (!fs::exists(directory) || !fs::is_directory(directory))
    {
        std::cerr << "Missing directory or wrong path, creating new.\n";
        fs::create_directory(directory);
    }
    _folder_ root("Projects");
    walk_dir(root, directory,full_path);
    return root;
}