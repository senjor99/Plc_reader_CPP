#pragma once

#include <datatype.hpp>


namespace fs = std::filesystem;

namespace folders
{
    std::map<std::string, DbInfo> get_dbs();
    
    _folder_ get_instances();

    void walk_dir(_folder_& dir, const std::string& path, std::string& f_path);

};