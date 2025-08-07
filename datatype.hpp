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

class STD_SINGLE;
class STD_ARR_ELEM;
class STD_ARRAY;
class UDT_SINGLE;
class UDT_ARR_ELEM;
class UDT_ARRAY;
class STRUCT_SINGLE;
class UDT_RAW;
class DB;
class BASE;
class BASE_CONTAINER;
class Element;

struct FILTER;

using VariantElement = 
    std::variant<
        std::shared_ptr<STD_SINGLE>,
        std::shared_ptr<STD_ARR_ELEM>,
        std::shared_ptr<STD_ARRAY>,
        std::shared_ptr<STRUCT_SINGLE>,
        std::shared_ptr<UDT_SINGLE>,
        std::shared_ptr<UDT_ARR_ELEM>,
        std::shared_ptr<UDT_ARRAY>
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

struct DeviceInfo
{
    std::string ip = "";
    std::string raw_name;
    std::string profiname;
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

VariantElement create_element(const VariantElement& el,std::shared_ptr<BASE_CONTAINER> par);

std::string to_lowercase(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
};


namespace translate{    
    
    bool get_bool(const std::vector<unsigned char>& buffer, int byteOffset, int bitOffset) {

        return (buffer[byteOffset] >> bitOffset) & 0x01;
    }

    int get_int(const std::vector<unsigned char>& buffer, int offset, int length) {
        int result = 0;
        for (int i = 0; i < length; ++i) {
            result |= (buffer[offset + i] << ((length - i - 1) * 8));
        }

        return result;
    }

    std::string get_string(const std::vector<unsigned char>& buffer, int offset, int length) {
        if (offset + length > buffer.size()) {
            throw std::out_of_range("Buffer too small for string extraction");
        }

        return std::string(buffer.begin() + offset, buffer.begin() + offset + length);
    }

    Value generic_get(const std::vector<unsigned char>& buffer, std::pair<int,int>offset_in, const std::string& type_in) {
        std::string type_of = to_lowercase(type_in);
        auto it = type_size.find(type_of);
        if (it == type_size.end()) return 0;

        int length = it->second.first;

        if (type_of == "bool") {
            return get_bool(buffer, offset_in.first, offset_in.second);
        } else if (type_of == "string" || type_of =="char") {
            return get_string(buffer, offset_in.first, length);
        } else {
            return get_int(buffer, offset_in.first, length);
        }
    }

    bool parse_bool(std::string& bool_in)
    {   
        if(bool_in == "true")
            return true;
        else
            return false;
    }

    Value parse_type(std::string& input)
    {
       Value output;
        try {
        output = std::stoi(input);
        return output;

        } catch (const std::invalid_argument&) {
            // not an int
        } catch (const std::out_of_range&) {
            // out of int range
        }
        std::string input_low = to_lowercase(input); 
        if( input_low == "true" || input_low == "false")
            output = parse_bool(input_low);
        return input;
    }
};


class Element{
    protected:
        bool is_vis = true;
        std::shared_ptr<BASE_CONTAINER> parent;
    public:
        void set_parent(std::shared_ptr<BASE_CONTAINER> in ){parent = in;}
        std::shared_ptr<BASE_CONTAINER> get_parent()const {return parent;}
        virtual void set_vis(bool in) = 0;
        bool get_vis() const {return is_vis;}
};

class BASE_CONTAINER : public Element{
    protected:
    bool is_vis_set = false;
    std::string name;
    std::string type;
    std::vector<VariantElement> childs;
    std::vector<std::pair<std::string,VariantElement>> names;

    public:
    BASE_CONTAINER() = default;
    BASE_CONTAINER(std::string name_in,std::string type_in) : name(std::move(name_in)), type(std::move(type_in)){}
    
    std::string get_name()const{return name;}
    std::string get_type()const{return type;}
    const std::vector<VariantElement> get_childs() const {return childs;}
    
    void set_name(std::string name_in){name = std::move(name_in);}
    void set_type(std::string type_in){type = std::move(type_in);}
    void insert_child(VariantElement el){ childs.push_back(el);}
    
    void set_data_to_child(const std::vector<unsigned char>& buffer);
    void set_child_offset(std::pair<int,int>& actual_offset);
    void check_type_for_offset(VariantElement& elem,std::pair<int,int>& actual_offset);
    
    std::vector<std::pair<std::string,VariantElement>> get_names()const {return names;}
    void add_name(std::pair<std::string,VariantElement> el){names.push_back(el);}
    void set_vis(bool b_in) override;
};

class BASE : public Element
{
    protected:
    std::string name;
    std::string type; 
    std::pair<int, int> offset; 
    Value data = "-";

    public:
    BASE() = default;
    BASE(std::string name_in, std::string type_in)
    : name(std::move(name_in)), type(std::move(type_in)) {}
    
    std::string get_name() const{ return name;}
    std::string get_type() const{ return type;}
    std::pair<int,int> get_offset() const { return offset;}
    Value get_data() {return data;}
    
    
    void set_name(std::string name_in){ name = name_in;}
    void set_type(std::string type_in){ type = type_in;}
    void set_data(const std::vector<unsigned char>& buffer);
    void set_offset(std::pair<int,int>& offset_in);
    void set_vis(bool b_in) override ;
};
