#include <hw_interface.hpp>


std::vector<std::string> folders::extensions = {".db",".udt"};
std::string folders::directory = "./root";


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
            auto name = i.path().filename().string();
            //name = name.substr(0,name.find(".db"));
            _file_ new_file(name);
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