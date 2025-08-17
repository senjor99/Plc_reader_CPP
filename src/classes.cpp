#include <classes.hpp>

std::string to_lowercase(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
    [](unsigned char c) { return std::tolower(c); });
    return s;
};

std::unordered_map<std::string,std::pair<int,int>> tia_type_size {
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

template <class Pred>
bool Filter::walk_set_vis(VariantElement el, Pred pred) {
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

bool Filter::walk_udt_value(VariantElement el,
                        const std::string& udt_name,
                        const Value& val,
                        bool in_udt ) {
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

void Filter::ResetAll(std::shared_ptr<BASE_CONTAINER> db) 
{
    for (auto& ch : db->get_childs())
        std::visit([&](auto& ptr) {
            using T = std::decay_t<decltype(ptr)>;
            using E = typename T::element_type;

            if constexpr(std::is_base_of_v<BASE,E>){
                ptr->set_vis(true);
            }
            else if constexpr(std::is_base_of_v<BASE_CONTAINER,E>){
                ptr->set_vis(true);
                Filter::ResetAll(ptr);
            }
        },ch);
}


Filter::FILTER_VALUE::FILTER_VALUE(Value in) : value(in) {}
    
void Filter::FILTER_VALUE::set_filter(std::shared_ptr<DB> el) 
{
    for (auto& ch : el->get_childs())
        walk_set_vis(ch, [&](BASE& b)
        {
            if(b.get_data() == Value("")) return true;
            else return b.get_data() == value;
        });
}

    
Filter::FILTER_NAME::FILTER_NAME(std::string name_in) : name(name_in) {}

void Filter::FILTER_NAME::set_filter(std::shared_ptr<DB> el)  
{
    for (auto& ch : el->get_childs())
        walk_set_vis(ch, [&](BASE& b){ 
            if(b.get_data() == Value("")) return true;
            else return b.get_name() == name; 
        });
}

Filter::FILTER_VALUE_NAME::FILTER_VALUE_NAME(Value val_in, std::string n_in) 
    : value(val_in),name(n_in) {}

    void Filter::FILTER_VALUE_NAME::set_filter(std::shared_ptr<DB> el) 
    {
        for (auto& ch : el->get_childs())
            walk_set_vis(ch, [&](BASE& b){
                if(b.get_name() == "" && b.get_data() == Value("")) return true;
                return b.get_name() == name && b.get_data() == value;
            });
    }

Filter::FILTER_VALUE_UDT::FILTER_VALUE_UDT(Value val_in, std::string n_in) 
    : value(val_in),udt_name(n_in) {}



    void Filter::FILTER_VALUE_UDT::set_filter(std::shared_ptr<DB> el) {
        for (auto& ch : el->get_childs())
            walk_udt_value(ch, udt_name, value, false);
    }

    
std::shared_ptr<Filter::BASE_FILTER> Filter::Do_Filter(filterElem* el)
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





VariantElement class_utils::create_element(const VariantElement& el,std::shared_ptr<BASE_CONTAINER> par) {
    
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
        else if constexpr (std::is_same_v<T, std::shared_ptr<STRUCT_ARRAY>>){
            //throw std::logic_error("Unhandled type in create_element");
            return STRUCT_ARRAY::create_from_element(ptr->get_name(),ptr->get_type(),ptr->get_childs(),par,ptr->get_start(),ptr->get_end());
        } 
        else if constexpr (std::is_same_v<T, std::shared_ptr<STRUCT_ARRAY_EL>>){
            //throw std::logic_error("Unhandled type in create_element");
            return STRUCT_ARRAY_EL::create_from_element(ptr->get_name(),ptr->get_index(),ptr->get_max_index(),ptr->get_childs(),par);
        }     
        throw std::logic_error("Unhandled type in create_element");
        return{};
    
    }, el);
};

std::shared_ptr<UDT_RAW> class_utils::lookup_udt(std::string type_to_search,UdtRawMap map) {
    if(auto it = map.find(type_to_search);it != map.end()){
        return it->second;
    }
    else{
        return nullptr;
    }
}

void class_utils::apply_padding(std::pair<int,int>& offset_in){
    while(offset_in.first%2 !=0)
    {
        ++offset_in.first;
    }
    offset_in.second = 0;
};

void class_utils::check_offset(std::pair<int,int>& offset_in,std::pair<int,int>& size){
    if ( (offset_in.second + size.second > 8) ||
        ( size.second==0 && offset_in.second > 1)) {
        ++offset_in.first;
        offset_in.second = 0;
    }
    if ( size.first > 1 ) {
        class_utils::apply_padding(offset_in);
    }
};

std::pair<int,int> class_utils::get_size(std::string type){
    auto it = tia_type_size.find(to_lowercase(type));
    
    if(it == tia_type_size.end()){ 
        throw std::logic_error("Invalid element type");
    }
    return it->second;
};



bool translate::get_bool(const std::vector<unsigned char>& buffer, int byteOffset, int bitOffset) {

    return (buffer[byteOffset] >> bitOffset) & 0x01;
}

int translate::get_int(const std::vector<unsigned char>& buffer, int offset, int length) {
    int result = 0;
    for (int i = 0; i < length; ++i) {
        result |= (buffer[offset + i] << ((length - i - 1) * 8));
    }

    return result;
}

std::string translate::get_string(const std::vector<unsigned char>& buffer, int offset, int length) {
    if (offset + length > buffer.size()) {
        throw std::out_of_range("Buffer too small for string extraction");
    }

    return std::string(buffer.begin() + offset, buffer.begin() + offset + length);
}

Value translate::generic_get(const std::vector<unsigned char>& buffer, std::pair<int,int>offset_in, const std::string& type_in) {
    std::string type_of = to_lowercase(type_in);
    auto it = tia_type_size.find(type_of);
    if (it == tia_type_size.end()) return 0;

    int length = it->second.first;

    if (type_of == "bool") {
        return get_bool(buffer, offset_in.first, offset_in.second);
    } else if (type_of == "string" || type_of =="char") {
        return get_string(buffer, offset_in.first, length);
    } else {
        return get_int(buffer, offset_in.first, length);
    }
}

bool translate::parse_bool(std::string& bool_in)
{   
    if(bool_in == "true")
        return true;
    else
        return false;
}

Value translate::parse_type(std::string& input)
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
    name(name_in), type(type_in) {}

std::string BASE::get_name()const{return name;}
std::string BASE::get_type()const{return type;}
std::pair<int,int> BASE::get_offset() const { return offset;}
Value BASE::get_data() const{return data;}
bool BASE::get_vis() {return is_vis;}

void BASE::set_name(std::string name_in){ name = name_in;}
void BASE::set_type(std::string type_in){ type = type_in;}
void BASE::set_data(const std::vector<unsigned char>& buffer){data = translate::generic_get(buffer,offset,type);};
void BASE::set_vis(bool b_in) {is_vis = b_in;};
void BASE::set_offset(std::pair<int,int>& offset_in){
   std::pair<int,int> size = class_utils::get_size(type);

    class_utils::check_offset(offset_in,size);

    offset = offset_in;
    offset_in.first +=size.first;
    offset_in.second +=size.second;
};
// class BASE Functions

// class BASE_CONTAINER Functions
/*

*/
BASE_CONTAINER::BASE_CONTAINER(std::string name_in,std::string type_in) 
    : name(name_in), type(type_in){}

std::string BASE_CONTAINER::get_name()const{return name;}
std::string BASE_CONTAINER::get_type()const{return type;}
std::vector<VariantElement> BASE_CONTAINER::get_childs() const {return childs;}
std::vector<std::pair<std::string,VariantElement>> BASE_CONTAINER::get_names()const {return names;}
bool BASE_CONTAINER::get_vis() {return is_vis;}

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
            class_utils::apply_padding(actual_offset);
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
const std::vector<VariantElement>& UDT_RAW::get_childs() const { return childs;}

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
    : BASE_CONTAINER(n_in,t_in),index_start(st),index_end(end){this->set_parent(parent);}

std::shared_ptr<STD_ARRAY> STD_ARRAY::create_array
    (const std::string& n_in,const std::string& t_in,int st,int end,std::shared_ptr<BASE_CONTAINER> parent) 
    {   
        auto arr = std::make_shared<STD_ARRAY>();
        arr->name = n_in;
        arr->type = t_in;
        arr->index_start = st ;
        arr->index_end = end;
        
        auto it = tia_type_size.find(to_lowercase(t_in));
        if(it == tia_type_size.end()){ 
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
    :BASE_CONTAINER(name_in,type_in){}

std::shared_ptr<UDT_SINGLE> UDT_SINGLE::create_from_element
    (std::string n_in,std::string t_in,const std::vector<VariantElement>& el,std::shared_ptr<BASE_CONTAINER> parent)
    {
        std::shared_ptr<UDT_SINGLE> self = std::make_shared<UDT_SINGLE>(n_in,t_in);
        self->set_parent(parent);
        ElementInfo info;
        info.is_arr = false;
        for(auto i : el){
            auto new_el = class_utils::create_element(i,self);
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
    : UDT_SINGLE(el->get_name(),el->get_type()) 
    {
        index = el->get_index();
        max_index = el->get_max_index();
        childs = el->get_childs();
        parent = par;
    }

std::shared_ptr<UDT_ARR_ELEM> UDT_ARR_ELEM::create_from_element
    (std::shared_ptr<UDT_SINGLE> el,int arr_nr,int max_arr_nr,std::shared_ptr<BASE_CONTAINER> parent)
    {
        std::shared_ptr<UDT_ARR_ELEM> self = std::make_shared<UDT_ARR_ELEM>();
        std::string indexed_name= el->get_name()+"["+ std::to_string(arr_nr) +"]";
        self->set_name(indexed_name);
        self->set_type(el->get_type());
        self->set_parent(parent);
        
        for(auto i : el->get_childs()){
            auto new_el = class_utils::create_element(i,self);
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
    : BASE_CONTAINER(el->get_name(),el->get_type()) 
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

std::shared_ptr<UDT_ARRAY> UDT_ARRAY::create_from_element
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
        auto new_el = class_utils::create_element(i,self);
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
STRUCT_SINGLE::STRUCT_SINGLE( std::string& name_in,std::string type_in) 
    : BASE_CONTAINER(name_in,"Struct"){}

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
            auto new_el = class_utils::create_element(i,self);
            std::visit([&](auto&& ptr){ ptr->set_parent(self);},new_el);
            self->insert_child(new_el);
        }        
        return self;
    }
// Class STRUCT_SINGLE Functions

// Class STRUCT_ARRAY_EL Functions
/*

*/
STRUCT_ARRAY_EL::STRUCT_ARRAY_EL(std::string& n_in,int idx,int idx_max,std::string t_in) 
    : STRUCT_SINGLE(n_in,"Struct"),index(idx),max_index(idx_max){}

STRUCT_ARRAY_EL::STRUCT_ARRAY_EL() = default;

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
        std::shared_ptr<STRUCT_ARRAY_EL> self = std::make_shared<STRUCT_ARRAY_EL>(name_in,idx,idx_max,"Struct");
        self->set_parent(parent);
        ElementInfo info;
        info.is_arr = false;

        for(auto i : el){
            auto new_el = class_utils::create_element(i,self);
            std::visit([&](auto&& ptr){ ptr->set_parent(self);},new_el);
            self->insert_child(new_el);
        }        
        return self;
    }
// Class STRUCT_ARRAY_EL Functions

// Class STRUCT_ARRAY Functions
/*

*/
STRUCT_ARRAY::STRUCT_ARRAY( std::string& name_in,std::string type_in) 
    : BASE_CONTAINER(name_in,"Struct"){}

std::string STRUCT_ARRAY::get_name() const { return name; }
std::string STRUCT_ARRAY::get_type() const { return "Struct";}
int STRUCT_ARRAY::get_start()const{return start;}
int STRUCT_ARRAY::get_end()const{return end;}

std::shared_ptr<STRUCT_ARRAY> STRUCT_ARRAY::create_from_element
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
                auto new_el = class_utils::create_element(i,self);
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
DB::DB(std::string name_in) :BASE_CONTAINER(name_in,"DB"){};

std::string DB::get_name() const{return name;}
std::pair<int,int> DB::get_max_offset()const{return offset_max;}

void DB::set_max_offset(std::pair<int,int> ofst){offset_max = ofst;}
void DB::_set_offset(){set_child_offset(offset_max);}
void DB::_set_data(const std::vector<unsigned char>& buffer){set_data_to_child(buffer);}
// Class DB Functions


