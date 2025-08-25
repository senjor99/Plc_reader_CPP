#include <hw_interface.hpp>

/// Recursively walks a directory tree and populates a hierarchical folder/file model.
///
/// Builds a tree of `_folder_` and `_file_` nodes starting at `path`.  
/// For each subdirectory, creates a `_folder_`, updates its `path`, recurses into it,
/// then appends it to `dir.elements`. For each regular file, creates a `_file_`,
/// sets its `path`, and appends it to `dir.elements`.
///
/// @note `f_path` is used as a mutable accumulator for the current path and is
///       modified in-place during recursion.
///       If you pass `f_path` by reference (as here), remember to restore it
///       after the recursive call to avoid path growth across siblings.
///       Consider passing `f_path` by value or using a local guard to restore it.
///
/// @param[out] dir     Destination node representing the current folder; will receive children.
/// @param[in]  path    Filesystem path to traverse (on disk).
/// @param[in,out] f_path Accumulated logical path stored in the model; updated during traversal.
void folders::walk_dir(_folder_& dir, const std::string& path, std::string& f_path)
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

/// Returns the root folder model by scanning (or creating) the base directory.
///
/// Uses the current working directory as base and ensures a `root` subdirectory exists.
/// Builds a `_folder_` tree labeled "Projects" by walking `<cwd>/root`.
///
/// @return A fully-populated `_folder_` representing the project tree.
///
/// @note The on-disk directory is `<cwd>/root`, while the user-facing label is "Projects".
///       Adjust names if you want the label to mirror the physical directory name.
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

