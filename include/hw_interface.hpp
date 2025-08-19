#pragma once

#include <datatype.hpp>
#include <cctype>

namespace fs = std::filesystem;

namespace ethernet
{
    std::map<std::string, std::shared_ptr<DeviceProfInfo>> get_devices(std::string& sub);
    bool _GetCpuInfo(std::string& ip,TS7CpuInfo& name);
    bool _ping(std::string& ip);
};
namespace folders
{
    std::map<std::string, DbInfo> get_dbs();
    bool isNumber(char c);
    
    _folder_ get_instances();

    void _erase_extensions(std::string& ph);
    std::pair<int,std::string>  _get_name(std::string ph);
    extern std::vector<std::string> extensions;
    extern std::string directory;
};