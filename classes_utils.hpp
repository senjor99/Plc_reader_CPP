#pragma once 

#include <classes.hpp>
#include <cxxabi.h>

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


void BASE::set_offset(std::pair<int,int>& offset_in){
   // Retrieving the standard type size
   std::pair<int,int> size = get_size(type);

    check_offset(offset_in,size);

    offset = offset_in;
    offset_in.first +=size.first;
    offset_in.second +=size.second;

    
};

void BASE::set_vis(bool b_in)
{
    is_vis = b_in;
    if(parent)
        parent->set_vis(b_in);
    else
        if (dynamic_cast<std::shared_ptr<DB>*>(this) != nullptr)  
            throw std::logic_error("No Parent on element "+name);
};

void BASE::set_data(const std::vector<unsigned char>& buffer){
    data = translate::generic_get(buffer,offset,type);
};

void BASE_CONTAINER::set_child_offset(std::pair<int,int>& actual_offset){
    for(auto& i : childs){
            check_type_for_offset(i,actual_offset);
        }
};

void BASE_CONTAINER::check_type_for_offset(VariantElement& elem,std::pair<int,int>& actual_offset)
{
    std::visit([&](auto&& ptr) {
        using T = std::decay_t<decltype(ptr)>;

        if constexpr (std::is_same_v<T, std::shared_ptr<STD_SINGLE>>||
                    std::is_same_v<T, std::shared_ptr<STD_ARR_ELEM>>){
            
            ptr->set_offset(actual_offset);
        }
        else if constexpr (
            std::is_same_v<T, std::shared_ptr<UDT_ARRAY>> ||
            std::is_same_v<T, std::shared_ptr<UDT_SINGLE>> ||
            std::is_same_v<T, std::shared_ptr<STD_ARRAY>>  ||
            std::is_same_v<T, std::shared_ptr<UDT_ARR_ELEM>> ||
            std::is_same_v<T, std::shared_ptr<STRUCT_SINGLE>>
        ) {
            if(actual_offset.second>1){
                ++actual_offset.first;
                actual_offset.second = 0;
            }
            apply_padding(actual_offset);
            ptr->set_child_offset(actual_offset);

        }
    },elem);
};

void BASE_CONTAINER::set_data_to_child(const std::vector<unsigned char>& buffer){
    for(auto i : childs){
        std::visit([&](auto&& ptr) {
        using T = std::decay_t<decltype(ptr)>;

        if constexpr (std::is_same_v<T, std::shared_ptr<STD_SINGLE>>||
                    std::is_same_v<T, std::shared_ptr<STD_ARR_ELEM>>){
            
            ptr->set_data(buffer);
        }
        else if constexpr (
            std::is_same_v<T, std::shared_ptr<UDT_ARRAY>> ||
            std::is_same_v<T, std::shared_ptr<UDT_SINGLE>> ||
            std::is_same_v<T, std::shared_ptr<STD_ARRAY>>  ||
            std::is_same_v<T, std::shared_ptr<UDT_ARR_ELEM>> ||
            std::is_same_v<T, std::shared_ptr<STRUCT_SINGLE>>
        ) {ptr->set_data_to_child(buffer) ;}
    },i);
    }
};

void BASE_CONTAINER::set_vis(bool b_in)
{
    if(b_in)
    {
        is_vis = b_in;
        is_vis_set = true;
    }
    else if(!b_in &&!is_vis_set)
        is_vis = b_in;
    if(parent) parent->set_vis(b_in);
    else
        if (dynamic_cast<std::shared_ptr<DB>*>(this) != nullptr)  
            throw std::logic_error("No Parent on element "+name);


};
namespace Filter
{
    struct filterElem 
    {
        std::optional<Value> value_in;
        std::optional<std::string> name;
        std::optional<Value> udt_value;
        std::optional<std::string> udt_name;
    };

    class BASE_FILTER
    {
    public:
        virtual void set_filter(std::shared_ptr<DB> el) = 0;
        virtual ~BASE_FILTER() = default;
    };

    class FILTER_VALUE : public BASE_FILTER
    {
    public:        
        Value value;

        FILTER_VALUE(Value in) : value(in) {}

        void set_filter(std::shared_ptr<DB> el) override 
        {
            for(auto i : el->get_childs()) 
            {
                _set_filter(i,value);
            }
        }
        static void _set_filter(VariantElement el,Value val_in)
        {
            std::visit([&](auto&& ptr) 
            {
                using T = std::decay_t<decltype(ptr)>;
                
                if constexpr (std::is_base_of_v<BASE, typename T::element_type>)
                {
                    /*
                    if (std::holds_alternative<std::string>(ptr->get_data()))
                        std::cout<<"string "<<ptr->get_name()<<"\n";
                    if (std::holds_alternative<int>(ptr->get_data()))
                        std::cout<<"int "<<ptr->get_name()<<"\n";
                    */
                    ptr->set_vis(ptr->get_data() == val_in);
                }
                else if constexpr (std::is_base_of_v<BASE_CONTAINER, typename T::element_type>)
                {
                    for(auto i : ptr->get_childs()) _set_filter(i,val_in);
                }
            },el);
        }
    };

    class FILTER_NAME : public BASE_FILTER
    {
    public:
        std::string name;
        
        FILTER_NAME(std::string name_in) : name(name_in) {}

        void set_filter(std::shared_ptr<DB> el) override 
        {
            for(auto i : el->get_childs()){
                _set_filter(i,name);
            }
        }
        static void _set_filter(VariantElement el,std::string in)
        {
            std::visit([&](auto&& ptr) 
            {
                using T = std::decay_t<decltype(ptr)>;
                
                if constexpr (  std::is_base_of_v<T, std::shared_ptr<BASE>>)
                {
                    ptr->set_vis(ptr->get_name() == in);
                }
                else if constexpr (  std::is_base_of_v<T, std::shared_ptr<BASE_CONTAINER>>)
                {
                   for(auto i : ptr->get_childs()) _set_filter(i,in);
                }
            },el);
        }
    };

    class FILTER_VALUE_NAME: public BASE_FILTER
    {
        public:
        Value value;
        std::string name;
        FILTER_VALUE_NAME(Value val_in, std::string n_in) 
            : value(val_in),name(n_in) {}

        void set_filter(std::shared_ptr<DB> el) override 
        {
            for(auto i : el->get_childs()) { _set_filter(i,name,value);}
        }
        static void _set_filter(VariantElement el,std::string n_in,Value val_in)
        {
            std::visit([&](auto&& ptr) 
            {
                using T = std::decay_t<decltype(ptr)>;
                
                if constexpr (  std::is_base_of_v<T, std::shared_ptr<BASE>>)
                {
                    
                    ptr->set_vis(ptr->get_name() == n_in && ptr->get_data() == val_in);
                }
                else if constexpr (  std::is_base_of_v<T, std::shared_ptr<BASE_CONTAINER>>)
                {
                    for(auto i : ptr->get_childs()) _set_filter(i,n_in,val_in);
                }
            },el);
        }
    };
    
    class FILTER_VALUE_UDT: public BASE_FILTER
    {
        public:
        Value value;
        std::string udt_name;
        FILTER_VALUE_UDT(Value val_in, std::string n_in) 
            : value(val_in),udt_name(n_in) {}

        void set_filter(std::shared_ptr<DB> el) override 
        {
            for(auto i : el->get_childs()) 
            {
                _set_filter(i,udt_name,value,false);
            }
        }
        static void _set_filter(VariantElement el,std::string n_in,Value val_in,bool x)
        {
            std::visit([&](auto&& ptr) 
            {
                using T = std::decay_t<decltype(ptr)>;
                
                if constexpr (  std::is_base_of_v<T, std::shared_ptr<BASE>>)
                {
                    
                    ptr->set_vis(ptr->get_data() == val_in);
                }
                else if constexpr (  std::is_base_of_v<T, std::shared_ptr<BASE_CONTAINER>>)
                {
                    if(ptr->get_name() == n_in || x) 
                        for(auto i : ptr->get_childs()) 
                            _set_filter(i,n_in,val_in,true);
                }
            },el);
        }

    };
     
    std::shared_ptr<BASE_FILTER> Do_Filter(filterElem* el)
    {
        if(el->value_in.has_value() && !el->name.has_value() && !el->udt_name.has_value() && !el->udt_value.has_value()){
            return std::make_shared<FILTER_VALUE>(el->value_in.value());
        }
        else if(!el->value_in.has_value() && el->name.has_value() && !el->udt_name.has_value() && !el->udt_value.has_value()){
            return std::make_shared<FILTER_NAME>(el->name.value());
        }
        else if(el->value_in.has_value() && el->name.has_value() && !el->udt_name.has_value() && !el->udt_value.has_value()){
            return std::make_shared<FILTER_VALUE_NAME>(el->value_in.value(),el->name.value());
        }
        else if(!el->value_in.has_value() && !el->name.has_value() && el->udt_name.has_value() && el->udt_value.has_value()){
            return std::make_shared<FILTER_VALUE_UDT>(el->udt_value.value(),el->udt_name.value());
        }
        else if(!el->value_in.has_value() && !el->name.has_value() && !el->udt_name.has_value() && !el->udt_value.has_value()){
            return nullptr;
        }
        else{
            throw std::logic_error("Invalid combination of arguments in Do_Filter");
        }
    };
};
