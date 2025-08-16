#pragma once 

#include <hw_interface.hpp>
#include <cstdlib>

namespace ethernet
{
            
    std::map<std::string, std::shared_ptr<DeviceProfInfo>> get_devices(std::string& sub )
    {
        std::map<std::string,std::shared_ptr<DeviceProfInfo>> res;
        std::shared_ptr<DeviceProfInfo> DevInfo;
        TS7CpuInfo cp_res ;

        for(int i = 0; i<255;i++)
        {
            std::string ip = sub+std::to_string(i); 
            
            if(_ping(ip)) if(_GetCpuInfo(ip,&cp_res))
            {
                    DevInfo = DeviceProfInfo::ptr_from_S7CPUinfo(&cp_res,ip);
                    res[DevInfo->profiname] = DevInfo;
            }
        }
        return res;
    }

    bool _GetCpuInfo(std::string& ip,PS7CpuInfo name)
    {
        TS7Client cli ;
        if(cli.ConnectTo(ip.c_str(),0,1)==0) {
            
            cli.GetCpuInfo(name);
            cli.Disconnect();
            return true;
        }

        return false;
    }
    bool _ping(std::string& ip)
    {
        std::string cmd = "ping -n 1 -w 100 " + ip + " >nul 2>&1";
        return system(cmd.c_str()) == 0;
    }

};

namespace folders
{

    std::map<std::string, DbInfo> get_dbs()
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

    void _erase_extensions(std::vector<std::string>& phs)
    {
        for(auto& ph : phs)
        {
            ph.substr(0, ph.find(".db"));
            ph.substr(0, ph.find(".udt"));
        }
    } 
    std::vector<std::string> _erase_base(std::vector<std::string> phs)
    {
        std::string base = std::filesystem::current_path().string();
        for(auto ph : phs)
        {
            ph.substr(0, ph.find("base"));
        }
        return phs;
    } 

};