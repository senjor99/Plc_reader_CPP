#pragma once

#include <datatype.hpp>

namespace fs = std::filesystem;

namespace ethernet
{
    std::map<std::string, std::shared_ptr<DeviceProfInfo>> get_devices(std::string& sub );
    bool _GetCpuInfo(std::string& ip,TS7CpuInfo& name);
    bool _ping(std::string& ip);
};
namespace folders
{
    
    std::map<std::string, DbInfo> get_dbs();
    
    void _erase_extensions(std::vector<std::string>& phs);
    std::vector<std::string> _erase_base(std::vector<std::string> phs);
    
    extern std::vector<std::string> extensions;
    extern std::string directory;
};