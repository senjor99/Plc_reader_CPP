#pragma once

#include <datatype.hpp>


namespace fs = std::filesystem;

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