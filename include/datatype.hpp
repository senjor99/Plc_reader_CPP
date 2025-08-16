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
#include "snap7.cpp"

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
        std::shared_ptr<STRUCT_ARRAY>,
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
    static std::shared_ptr<DeviceProfInfo> ptr_from_S7CPUinfo(PS7CpuInfo in,std::string _ip)
    {
        std::shared_ptr<DeviceProfInfo> res;
        res->ip = _ip;
        res->raw_name = in->ASName;
        res->profiname = in->ModuleName;
        return res;
    }
};

struct DbInfo
{
    std::string name = "";
    std::string path;
    int default_number;
};

std::unordered_map<std::string,std::pair<int,int>> type_size {
    {"bool",   {  0, 1}},  
    {"byte",   {  1, 0}}, 
    {"char",   {  1, 0}},
    {"word",   {  2, 0}}, 
    {"int",    {  2, 0}},
    {"dint",   {  4, 0}}, 
    {"real",   {  4, 0}},
    {"string", {256, 0}},
    {"date",   {  2, 0}},
    {"dword",  {  4, 0}},
    {"lreal",  {  8, 0}},
    {"sint",   {  1, 0}},
    {"time",   {  4, 0}},
    {"udint",  {  4, 0}},
    {"uint:",  {  2, 0}},
    {"usint",  {  1, 0}}
};

using Offset = std::pair<int, int>;

enum class Mode { None, Value, Name, ValueName, UdtValue };


std::string to_lowercase(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
    [](unsigned char c) { return std::tolower(c); });
    return s;
};

namespace classes_utils
{
    VariantElement create_element(const VariantElement& el,std::shared_ptr<BASE_CONTAINER> par);

    std::shared_ptr<UDT_RAW> lookup_udt(std::string type_to_search,UdtRawMap map);
    
    void apply_padding(std::pair<int,int>& offset_in);
    
    void check_offset(std::pair<int,int>& offset_in,std::pair<int,int>& size);

    std::pair<int,int> get_size(std::string type);
}