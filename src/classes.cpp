#pragma once 

#include <function.cpp>
#include <cxxabi.h>

using namespace class_utils;
// class Element Functions
/*
    The Main virtual class for all type raffigurating the Tia type
*/
void Element::set_parent(std::shared_ptr<BASE_CONTAINER> in ){parent = in;}
std::shared_ptr<BASE_CONTAINER> Element::get_parent()const {return parent;}
// class Element Functions

// class BASE Functions
/*

*/
BASE::BASE(std::string name_in, std::string type_in) :
    name(std::move(name_in)), type(std::move(type_in)) {}

std::string BASE::get_name()const{return name;}
std::string BASE::get_type()const{return type;}
std::pair<int,int> BASE::get_offset() const { return offset;}
Value BASE::get_data() const{return data;}

void BASE::set_name(std::string name_in){ name = name_in;}
void BASE::set_type(std::string type_in){ type = type_in;}
void BASE::set_data(const std::vector<unsigned char>& buffer){data = translate::generic_get(buffer,offset,type);};
void BASE::set_vis(bool b_in) {is_vis = b_in;};
void BASE::set_offset(std::pair<int,int>& offset_in){
   std::pair<int,int> size = get_size(type);

    check_offset(offset_in,size);

    offset = offset_in;
    offset_in.first +=size.first;
    offset_in.second +=size.second;
};
// class BASE Functions

// class BASE_CONTAINER Functions
/*

*/
BASE_CONTAINER::BASE_CONTAINER(std::string name_in,std::string type_in) 
    : name(std::move(name_in)), type(std::move(type_in)){}

std::string BASE_CONTAINER::get_name()const{return name;}
std::string BASE_CONTAINER::get_type()const{return type;}
std::vector<VariantElement> BASE_CONTAINER::get_childs() const {return childs;}
std::vector<std::pair<std::string,VariantElement>> BASE_CONTAINER::get_names()const {return names;}

void BASE_CONTAINER::set_name(std::string name_in){name = std::move(name_in);}
void BASE_CONTAINER::set_type(std::string type_in){type = std::move(type_in);}
void BASE_CONTAINER::insert_child(VariantElement el){ childs.push_back(el);}
void BASE_CONTAINER::add_name(std::pair<std::string,VariantElement> el){names.push_back(el);}
void BASE_CONTAINER::set_vis(bool b_in){ is_vis = b_in; };

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
// class BASE_CONTAINER Functions

// Class UDT_RAW Functions
/*

*/
UDT_RAW::UDT_RAW(std::string name) :
    raw_name(name) {}

std::string UDT_RAW::get_name() const { return raw_name;}
const auto& UDT_RAW::get_childs() const { return childs;}

void UDT_RAW::insert_child(VariantElement el_to_add){ childs.push_back(el_to_add);}
void UDT_RAW::set_name(std::string n_in){raw_name = std::move(n_in);}
// Class UDT_RAW Functions

// Class STD_SINGLE Functions
/*

*/
STD_SINGLE::STD_SINGLE(std::string name_in,std::string type_in,std::shared_ptr<BASE_CONTAINER> parent)
    : BASE(std::move(name_in),std::move(type_in)) {this->set_parent(parent);}
    

// Class STD_SINGLE Functions

// Class STD_ARR_ELEM Functions
/*

*/
STD_ARR_ELEM::STD_ARR_ELEM(std::string name_in,std::string type_in,int nr,int max,std::shared_ptr<BASE_CONTAINER> parent)
        : STD_SINGLE(std::move(name_in),std::move(type_in),parent) , index(nr), max_index(max) {}

int STD_ARR_ELEM::get_index() const { return index; }
int STD_ARR_ELEM::get_max_index() const { return max_index; }

void STD_ARR_ELEM::set_index(int index_in){index = index_in;}
void STD_ARR_ELEM::set_max_index(int max_index){index = max_index;}
// Class STD_ARR_ELEM Functions

// Class STD_ARRAY Functions
/*

*/
STD_ARRAY::STD_ARRAY(const std::string& n_in,const std::string& t_in,int st,int end,std::shared_ptr<BASE_CONTAINER> parent)
    : BASE_CONTAINER(std::move(n_in),std::move(t_in)),index_start(st),index_end(end){this->set_parent(parent);}

std::shared_ptr<STD_ARRAY> STD_ARRAY::create_array
    (const std::string& n_in,const std::string& t_in,int st,int end,std::shared_ptr<BASE_CONTAINER> parent) 
    {   
        auto arr = std::make_shared<STD_ARRAY>();
        arr->name = n_in;
        arr->type = t_in;
        arr->index_start = st ;
        arr->index_end = end;
        
        auto it = type_size.find(to_lowercase(t_in));
        if(it == type_size.end()){ 
            std::cout <<"element name "<< to_lowercase(t_in)<< "\n";
            throw std::logic_error("Invalid element type");
        }
      
        for (int i = st; i <= end; ++i) {
            std::string indexed_name = n_in + "[" + std::to_string(i) + "]";
            VariantElement el = std::make_shared<STD_ARR_ELEM>(indexed_name, t_in, i,end,arr);
            arr->childs.push_back(el);
        }
        return arr;
    }
// Class STD_ARRAY Functions

// Class UDT_SINGLE Functions
/*

*/
UDT_SINGLE::UDT_SINGLE(std::string name_in ,std::string type_in)
    :BASE_CONTAINER(std::move(name_in),std::move(type_in)){}

static std::shared_ptr<UDT_SINGLE> create_from_element
    (std::string n_in,std::string t_in,const std::vector<VariantElement>& el,std::shared_ptr<BASE_CONTAINER> parent)
    {
        std::shared_ptr<UDT_SINGLE> self = std::make_shared<UDT_SINGLE>(n_in,t_in);
        self->set_parent(parent);
        ElementInfo info;
        info.is_arr = false;
        for(auto i : el){
            auto new_el =create_element(i,self);
            std::visit([&](auto&& ptr){ ptr->set_parent(self);},new_el);
            self->insert_child(new_el);
        }
        return self;          
    }
// Class UDT_SINGLE Functions

// Class UDT_ARR_ELEM Functions
/*

*/
UDT_ARR_ELEM::UDT_ARR_ELEM(std::shared_ptr<UDT_ARR_ELEM> el,std::shared_ptr<BASE_CONTAINER> par)
    : UDT_SINGLE(std::move(el->get_name()),std::move(el->get_type())) 
    {
        index = el->get_index();
        max_index = el->get_max_index();
        childs = el->get_childs();
        parent = par;
    }

static std::shared_ptr<UDT_ARR_ELEM> create_from_element
    (std::shared_ptr<UDT_SINGLE> el,int arr_nr,int max_arr_nr,std::shared_ptr<BASE_CONTAINER> parent)
    {
        std::shared_ptr<UDT_ARR_ELEM> self = std::make_shared<UDT_ARR_ELEM>();
        std::string indexed_name= el->get_name()+"["+ std::to_string(arr_nr) +"]";
        self->set_name(indexed_name);
        self->set_type(el->get_type());
        self->set_parent(parent);
        
        for(auto i : el->get_childs()){
            auto new_el = create_element(i,self);
            std::visit([&](auto&& ptr){ ptr->set_parent(self);},new_el);
            self->insert_child(new_el);
        }
        return self;
    }
// Class UDT_ARR_ELEM Functions

// Class UDT_ARRAY Functions
/*

*/
UDT_ARRAY::UDT_ARRAY(std::shared_ptr<UDT_ARRAY> el,std::shared_ptr<BASE_CONTAINER> par)
    : BASE_CONTAINER(std::move(el->get_name()),std::move(el->get_type())) 
    {
        index_start = el->get_start();
        index_end = el->get_end();
        childs = el->get_childs();
        parent = par;
    }

    
std::string UDT_ARRAY::get_name()const{return name;};
std::string UDT_ARRAY::get_type()const{return type;}
int UDT_ARRAY::get_start()const{return index_start;}
int UDT_ARRAY::get_end()const{return index_end;}

static std::shared_ptr<UDT_ARRAY> create_from_element
(
    std::string name_in,
    std::string type_in,
    const std::vector<VariantElement> el,
    int start,
    int end,
    std::shared_ptr<BASE_CONTAINER> parent
)
{
    std::shared_ptr<UDT_ARRAY> self = std::make_shared<UDT_ARRAY>();
    self->set_name(name_in);
    self->set_type(type_in);
    self->set_parent(parent);

    std::shared_ptr<UDT_SINGLE> base_el = std::make_shared<UDT_SINGLE>(name_in,type_in);
    base_el->set_parent(self);
    for(auto i : el){
        auto new_el = create_element(i,self);
        std::visit([&](auto&& ptr){ ptr->set_parent(self);},new_el);
        base_el->insert_child(new_el);
    }   

    for(auto i = start; i <= end;++i){
        auto container = UDT_ARR_ELEM::create_from_element(base_el,i,end,self);
        self->insert_child(container);
    }
    return self;     
}
// Class UDT_ARRAY Functions

// Class STRUCT_SINGLE Functions
/*

*/
STRUCT_SINGLE::STRUCT_SINGLE( std::string& name_in,std::string type_in="Struct") 
    : BASE_CONTAINER(std::move(name_in),std::move(type_in)){}

std::string STRUCT_SINGLE::get_name() const { return name; }
std::string STRUCT_SINGLE::get_type() const { return "Struct";}
    
std::shared_ptr<STRUCT_SINGLE> STRUCT_SINGLE::create_from_element
    (std::string n_in,std::string t_in,const std::vector<VariantElement> el,std::shared_ptr<BASE_CONTAINER> parent)
    {
        std::shared_ptr<STRUCT_SINGLE> self = std::make_shared<STRUCT_SINGLE>(n_in,t_in);
        self->set_parent(parent);
        ElementInfo info;
        info.is_arr = false;

        for(auto i : el){
            auto new_el = create_element(i,self);
            std::visit([&](auto&& ptr){ ptr->set_parent(self);},new_el);
            self->insert_child(new_el);
        }        
        return self;
    }
// Class STRUCT_SINGLE Functions

// Class STRUCT_ARRAY_EL Functions
/*

*/
STRUCT_ARRAY_EL::STRUCT_ARRAY_EL(std::string& n_in,int idx,int idx_max,std::string t_in="Struct") 
    : STRUCT_SINGLE(n_in,t_in),index(idx),max_index(idx_max){}

int STRUCT_ARRAY_EL::get_index()const{return index;}
int STRUCT_ARRAY_EL::get_max_index()const{return max_index;}

void STRUCT_ARRAY_EL::set_index(int i){index = i;}
void STRUCT_ARRAY_EL::set_max_index(int i){max_index = i;}

std::shared_ptr<STRUCT_ARRAY_EL> STRUCT_ARRAY_EL::create_from_element
    (
        std::string name_in,
        int idx,
        int idx_max,
        const std::vector<VariantElement> el,
        std::shared_ptr<BASE_CONTAINER> parent
    )
    {
        std::shared_ptr<STRUCT_ARRAY_EL> self = std::make_shared<STRUCT_ARRAY_EL>(name_in,idx,idx_max);
        self->set_parent(parent);
        ElementInfo info;
        info.is_arr = false;

        for(auto i : el){
            auto new_el = create_element(i,self);
            std::visit([&](auto&& ptr){ ptr->set_parent(self);},new_el);
            self->insert_child(new_el);
        }        
        return self;
    }
// Class STRUCT_ARRAY_EL Functions

// Class STRUCT_ARRAY Functions
/*

*/
STRUCT_ARRAY::STRUCT_ARRAY( std::string& name_in,std::string type_in="Struct") 
    : BASE_CONTAINER(std::move(name_in),std::move(type_in)){}

std::string STRUCT_ARRAY::get_name() const { return name; }
std::string STRUCT_ARRAY::get_type() const { return "Struct";}

std::shared_ptr<STRUCT_ARRAY> create_from_element
    (
        std::string name_in,
        std::string type_in,
        const std::vector<VariantElement> el,
        std::shared_ptr<BASE_CONTAINER> parent,
        int start,
        int end
    )
    {
        std::shared_ptr<STRUCT_ARRAY_EL> idx_self;
        std::shared_ptr<STRUCT_ARRAY> self;
        std::string idx_name;
        ElementInfo info;

        for(auto i = start; i <= end;++i)
        {   
            idx_name = name_in+"["+std::to_string(i)+"]";
            idx_self = STRUCT_ARRAY_EL::create_from_element(idx_name,i,end,el,self);
            info.is_arr = true;

            for(auto i : el)
            {
                auto new_el = create_element(i,self);
                std::visit([&](auto&& ptr){ ptr->set_parent(self);},new_el);
                self->insert_child(new_el);
            }        
        }
        return self;
    }
// Class STRUCT_ARRAY Functions

// Class DB Functions
/*

*/
DB::DB(std::string name_in) :BASE_CONTAINER(std::move(name_in),"DB"){};

std::string DB::get_name() const{return name;}
std::pair<int,int> DB::get_max_offset()const{return offset_max;}

void DB::set_max_offset(std::pair<int,int> ofst){offset_max = ofst;}
void DB::_set_offset(){set_child_offset(offset_max);}
void DB::_set_data(const std::vector<unsigned char>& buffer){set_data_to_child(buffer);}
// Class DB Functions

namespace Filter
{
    struct filterElem 
    {
        std::optional<Value> value_in;
        std::optional<std::string> name;
        std::optional<Value> udt_value;
        std::optional<std::string> udt_name;
    };

    template <class Pred>
    static bool walk_set_vis(VariantElement el, Pred pred) {
        return std::visit([&](auto& ptr) -> bool {
            using T = std::decay_t<decltype(ptr)>;
            using E = typename T::element_type;

            if constexpr (std::is_base_of_v<BASE, E>) 
            {
                bool match = pred(*ptr);
                ptr->set_vis(match);
                
                return match;
            } 
            else if constexpr (std::is_base_of_v<BASE_CONTAINER, E>) 
            {
                bool any = false;

                for (auto& ch : ptr->get_childs())
                    any |= walk_set_vis(ch, pred);
                ptr->set_vis(any);

                return any;
            } 
            else
            {
                static_assert(!sizeof(E*), "Type exception in filter");
            }
        }, el);
    }

    static bool walk_udt_value(VariantElement el,
                           const std::string& udt_name,
                           const Value& val,
                           bool in_udt = false) {
        return std::visit([&](auto& ptr) -> bool {
            using T = std::decay_t<decltype(ptr)>;
            using E = typename T::element_type;

            if constexpr (std::is_base_of_v<BASE, E>) 
            {
                bool match = in_udt && (ptr->get_data() == val);
                ptr->set_vis(match);
                return match;

            } 
            else if constexpr (std::is_base_of_v<BASE_CONTAINER, E>) {
                bool here = in_udt || (ptr->get_name() == udt_name);
                bool any  = false;
                for (auto& ch : ptr->get_childs())
                    any |= walk_udt_value(ch, udt_name, val, here);

                    ptr->set_vis(here && any);
                return here && any;
            }
        }, el);
    }

    inline void ResetAll(std::shared_ptr<BASE_CONTAINER> db) {
        for (auto& ch : db->get_childs())
            std::visit([&](auto& ptr) {
                using T = std::decay_t<decltype(ptr)>;
                using E = typename T::element_type;

                if constexpr(std::is_base_of_v<BASE,E>){
                    ptr->set_vis(true);
                }
                else if constexpr(std::is_base_of_v<BASE_CONTAINER,E>){
                    ptr->set_vis(true);
                    ResetAll(ptr);
                }
            },ch);
    }

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
        
        void set_filter(std::shared_ptr<DB> el) override {
            for (auto& ch : el->get_childs())
                walk_set_vis(ch, [&](BASE& b)
                {
                    if(b.get_data() == Value("")) return true;
                    else return b.get_data() == value;
                });
        }
    };
        
    class FILTER_NAME : public BASE_FILTER
    {
    public:
        std::string name;
        
        FILTER_NAME(std::string name_in) : name(name_in) {}

        void set_filter(std::shared_ptr<DB> el) override {
            for (auto& ch : el->get_childs())
                walk_set_vis(ch, [&](BASE& b){ 
                    if(b.get_data() == Value("")) return true;
                    else return b.get_name() == name; 
                });
        }
    };

    class FILTER_VALUE_NAME: public BASE_FILTER
    {
        public:
        Value value;
        std::string name;
        FILTER_VALUE_NAME(Value val_in, std::string n_in) 
            : value(val_in),name(n_in) {}
        void set_filter(std::shared_ptr<DB> el) override {
            for (auto& ch : el->get_childs())
                walk_set_vis(ch, [&](BASE& b){
                    if(b.get_name() == "" && b.get_data() == Value("")) return true;
                    return b.get_name() == name && b.get_data() == value;
        });
        }
    };
    
    class FILTER_VALUE_UDT: public BASE_FILTER
    {
        public:
        Value value;
        std::string udt_name;
        FILTER_VALUE_UDT(Value val_in, std::string n_in) 
            : value(val_in),udt_name(n_in) {}

        void set_filter(std::shared_ptr<DB> el) override {
            for (auto& ch : el->get_childs())
                walk_udt_value(ch, udt_name, value, false);
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
        else if((!el->value_in.has_value() && !el->name.has_value()) && el->udt_name.has_value() || el->udt_value.has_value()){
            std::string udt_name = el->udt_name.has_value() ? el->udt_name.value() : "";
            Value udt_value = el->udt_value.has_value() ? el->udt_value.value() : "";

            return std::make_shared<FILTER_VALUE_UDT>(udt_value,udt_name);
        }
        else if(!el->value_in.has_value() && !el->name.has_value() && !el->udt_name.has_value() && !el->udt_value.has_value()){
            return nullptr;
        }
        else{
            std::cout<<"value: "<<el->value_in.has_value()<<"name: "<<el->name.has_value()<<"udt_name: "<<el->udt_name.has_value()<<"udt_value: "<<el->udt_value.has_value()<<"\n"; 
            throw std::logic_error("Invalid combination of arguments in Do_Filter");
        }
    };
};
