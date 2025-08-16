#include <classes.hpp>

namespace class_utils
{

    VariantElement create_element(const VariantElement& el,std::shared_ptr<BASE_CONTAINER> par) {
        
        return std::visit([&par](auto&& ptr) -> VariantElement {
            using T = std::decay_t<decltype(ptr)>;

            if constexpr (std::is_same_v<T, std::shared_ptr<STD_SINGLE>>){
                return std::make_shared<STD_SINGLE>(ptr->get_name(), ptr->get_type(),par);
            }
            if constexpr (std::is_same_v<T, std::shared_ptr<STD_ARR_ELEM>>){
                return std::make_shared<STD_ARR_ELEM>(ptr->get_name(), ptr->get_type(),ptr->get_index(),ptr->get_max_index(),par);
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<STD_ARRAY>>){
                return STD_ARRAY::create_array(ptr->get_name(), ptr->get_type(), ptr->get_start(), ptr->get_end(),par);                
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<UDT_SINGLE>>){
                return UDT_SINGLE::create_from_element(ptr->get_name(),ptr->get_type(),ptr->get_childs(),par);
            } 
            else if constexpr (std::is_same_v<T, std::shared_ptr<UDT_ARR_ELEM>>){
                
                return std::make_shared<UDT_ARR_ELEM>(ptr,par);
            }     
            else if constexpr (std::is_same_v<T, std::shared_ptr<UDT_ARRAY>>){
                return std::make_shared<UDT_ARRAY>(ptr,par);
            }
            else if constexpr (std::is_same_v<T, std::shared_ptr<STRUCT_SINGLE>>){
                return STRUCT_SINGLE::create_from_element(ptr->get_name(),ptr->get_type(),ptr->get_childs(),par);
            }   
            else if constexpr (std::is_same_v<T, std::shared_ptr<STRUCT_SINGLE>>){
                return STRUCT_ARRAY::create_from_element(ptr->get_name(),ptr->get_type(),ptr->get_childs(),par);
            } 
            else if constexpr (std::is_same_v<T, std::shared_ptr<STRUCT_SINGLE>>){
                return STRUCT_ARRAY_EL::create_from_element(ptr->get_name(),ptr->get_type(),ptr->get_childs(),par);
            }     
            throw std::logic_error("Unhandled type in create_element");
            return{};
        
        }, el);
    };

    std::shared_ptr<UDT_RAW> lookup_udt(std::string type_to_search,UdtRawMap map) {
        if(auto it = map.find(type_to_search);it != map.end()){
            return it->second;
        }
        else{
            return nullptr;
        }
    }

    void apply_padding(std::pair<int,int>& offset_in){
        while(offset_in.first%2 !=0)
        {
            ++offset_in.first;
        }
        offset_in.second = 0;
    };

    void check_offset(std::pair<int,int>& offset_in,std::pair<int,int>& size){
        if ( (offset_in.second + size.second > 8) ||
            ( size.second==0 && offset_in.second > 1)) {
            ++offset_in.first;
            offset_in.second = 0;
        }
        if ( size.first > 1 ) {
            apply_padding(offset_in);
        }
    };

    std::pair<int,int> get_size(std::string type){
        auto it = type_size.find(to_lowercase(type));
        
        if(it == type_size.end()){ 
            throw std::logic_error("Invalid element type");
        }
        return it->second;
    };

}
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

