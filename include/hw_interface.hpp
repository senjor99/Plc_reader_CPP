#pragma once

#include <gui.cpp>

namespace fs = std::filesystem;

namespace ethernet
{

    static std::map<std::string, std::shared_ptr<DeviceProfInfo>> get_devices(std::string& sub );
    bool _GetCpuInfo(std::string& ip,PS7CpuInfo& name);
    bool _ping(std::string& ip);

};
namespace folders
{
    
    std::map<std::string, DbInfo> get_dbs();
    
    void _erase_extension();
    std::vector<std::string> _erase_base(std::vector<std::string> phs);
    
    std::vector<std::string> extensions = {".db",".udt"};
    std::string directory = "./instances";
};