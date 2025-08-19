#pragma once

#include <iostream>
#include <string>
#include <variant>
#include <vector>
#include <list>
#include <memory>
#include <map>
#include <unordered_map>
#include <optional>
#include <algorithm>
#include <filesystem> 
#include <array>
#include <cstdlib>
#include "snap7.h"

class Element;
class BASE;
class BASE_CONTAINER;
class STD_SINGLE;
class STD_ARR_ELEM;
class STD_ARRAY;
class UDT_SINGLE;
class UDT_ARR_ELEM;
class UDT_ARRAY;
class STRUCT_SINGLE;
class STRUCT_ARRAY_EL;
class STRUCT_ARRAY;
class UDT_RAW;
class DB;

struct FILTER;

using VariantElement = 
    std::variant<
        std::shared_ptr<STD_SINGLE>,
        std::shared_ptr<STD_ARR_ELEM>,
        std::shared_ptr<STD_ARRAY>,
        std::shared_ptr<UDT_SINGLE>,
        std::shared_ptr<UDT_ARR_ELEM>,
        std::shared_ptr<UDT_ARRAY>,
        std::shared_ptr<STRUCT_SINGLE>,
        std::shared_ptr<STRUCT_ARRAY_EL>,
        std::shared_ptr<STRUCT_ARRAY>
    >;

using UdtRawMap = 
    std::map<
        std::string,
        std::shared_ptr<UDT_RAW>
    >;

using ScopeVariant =
    std::variant<
        std::shared_ptr<UDT_RAW>,
        std::shared_ptr<DB>,
        std::shared_ptr<STRUCT_SINGLE>
    >;

using Value = std::variant<int, bool, std::string>;

class _file_
{
public:
    std::string name;
    std::string path;
    _file_() = default;
    _file_(std::string n_in) : name(std::move(n_in)) {}
};

class _folder_;
using FileFolderVar = std::variant<_folder_, _file_>;

class _folder_
{
public:
    std::string name;
    std::string path;
    std::vector<FileFolderVar> elements;

    _folder_() = default;
    _folder_(std::string n_in) : name(std::move(n_in)) {}
};


struct ElementInfo{
    std::string indexed_name;
    std::string type;
    bool is_arr;
    int index_start;
    int index_end;
    std::shared_ptr<UDT_RAW> raw;
};

struct DeviceProfInfo
{
    std::string ip = "";
    std::string raw_name;
    std::string profiname;
    static std::shared_ptr<DeviceProfInfo> ptr_from_S7CPUinfo(TS7CpuInfo& in,std::string _ip)
    {
        std::shared_ptr<DeviceProfInfo> res;
        res->ip = _ip;
        res->raw_name = in.ASName;
        res->profiname = in.ModuleName;
        return res;
    }
};

struct DbInfo
{
    std::string name = "";
    std::string path;
    int default_number;
};

struct ParserState {
    
    UdtRawMap udt_database;
    std::vector<ScopeVariant> element_in_scope;
    std::shared_ptr<DB> db;
    std::string DB_name;
    std::string name;
    std::string type;
    bool is_arr;
    int array_start;
    int array_end;
    std::string udt_name;
    std::pair<int,int> offset = {0,0};
    
    void clear_state();

    std::shared_ptr<UDT_RAW> lookup_udt() const ;

    void insert_element_inscope();
};

template<typename Rule>
struct action {
    template<typename Input>
    static void apply(const Input&, ParserState&) {
    }
};

namespace Filter
{
    struct filterElem 
    {
        std::optional<Value> value_in;
        std::optional<std::string> name;
    };
};


using Offset = std::pair<int, int>;

enum class Mode { None, Value, Name, ValueName };


std::string to_lowercase(std::string s);
