#include <classes.hpp>

/// \brief Converts a string to lowercase in-place and returns it.
/// \param s Input string (copied by value).
/// \return Lowercased copy of \p s.
std::string to_lowercase(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
    [](unsigned char c) { return std::tolower(c); });
    return s;
};

/// \brief TIA type size lookup table: {byteCount, bitCount}.
/// \details Maps TIA basic types to byte/bit sizes used for offset calculations.
/// Example: {"bool",{0,1}}, {"int",{2,0}}, {"real",{4,0}}, {"string",{256,0}}.
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
    {"uint",  {  2, 0}},
    {"usint",  {  1, 0}}
};

/// \brief Recursively sets visibility based on a predicate over BASE nodes.
/// \tparam Pred Callable with signature \c bool(BASE&).
/// \param el Variant element (BASE or BASE_CONTAINER).
/// \param pred Predicate evaluated on leaves; containers are visible if any child matches.
/// \return true if the element (or any of its descendants) matches.
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

/// \brief Resets visibility to true for the whole subtree.
/// \param el Variant element root.
void Filter::reset_vis(VariantElement el)
{
    std::visit([&](auto& ptr) {
        using T = std::decay_t<decltype(ptr)>;
        using E = typename T::element_type;

        if constexpr(std::is_base_of_v<BASE,E>){
            ptr->set_vis(true);
        }
        else if constexpr(std::is_base_of_v<BASE_CONTAINER,E>){
            ptr->set_vis(true);
            for (auto& ch : ptr->get_childs())
                reset_vis(ch);
        }
        },el);
};

/// \brief Filter that clears any applied filtering (shows all).
/// \param el DB root to reset.
void Filter::RESET_FILTER::set_filter(std::shared_ptr<DB> el) 
{
    for (auto& ch : el->get_childs())
        Filter::reset_vis(ch);
};

/// \brief Constructs a value-based filter.
/// \param in Value to match on \c BASE::get_data().
Filter::FILTER_VALUE::FILTER_VALUE(Value in) : value(in) {}
    
/// \brief Applies a value-based filter on all leaves.
/// \details Matches nodes where \c get_data()==value or empty Value (passes through).
void Filter::FILTER_VALUE::set_filter(std::shared_ptr<DB> el) 
{
    for (auto& ch : el->get_childs())
        walk_set_vis(ch, [&](BASE& b)
        {
            if(b.get_data() == Value("")) return true;
            else return b.get_data() == value;
        });
}
    
/// \brief Constructs a name-based filter.
/// \param name_in Exact name to match on \c BASE::get_name().
Filter::FILTER_NAME::FILTER_NAME(std::string name_in) : name(name_in) {}

/// \brief Applies a name-based filter on all leaves.
/// \details Matches nodes where \c get_name()==name or empty Value (passes through).
void Filter::FILTER_NAME::set_filter(std::shared_ptr<DB> el)  
{
    for (auto& ch : el->get_childs())
        walk_set_vis(ch, [&](BASE& b){ 
            if(b.get_data() == Value("")) return true;
            else return b.get_name() == name; 
        });
}

/// \brief Constructs a combined value+name filter.
/// \param val_in Value to match.
/// \param n_in Name to match.
Filter::FILTER_VALUE_NAME::FILTER_VALUE_NAME(Value val_in, std::string n_in) 
    : value(val_in),name(n_in) {}

/// \brief Applies a combined value+name filter on all leaves.
/// \details Passes through empty name+value nodes; otherwise requires both matches.
void Filter::FILTER_VALUE_NAME::set_filter(std::shared_ptr<DB> el) 
{
    for (auto& ch : el->get_childs())
        walk_set_vis(ch, [&](BASE& b){
            if(b.get_name() == "" && b.get_data() == Value("")) return true;
            return b.get_name() == name && b.get_data() == value;
        });
}
    
/// \brief Factory that selects the appropriate filter implementation.
/// \param el Descriptor with optional name and value.
/// \return A concrete \c BASE_FILTER (value, name, value+name, or reset).
/// \throws std::logic_error on invalid combination.
std::shared_ptr<Filter::BASE_FILTER> Filter::Do_Filter(filterElem* el)
{
    if(el->value_in.has_value() && !el->name.has_value()){
        return std::make_shared<FILTER_VALUE>(el->value_in.value());
    }
    else if(!el->value_in.has_value() && el->name.has_value() ){
        return std::make_shared<FILTER_NAME>(el->name.value());
    }
    else if(el->value_in.has_value() && el->name.has_value() ){
        return std::make_shared<FILTER_VALUE_NAME>(el->value_in.value(),el->name.value());
    }
    else if(!el->value_in.has_value() && !el->name.has_value()){
        return std::make_shared<RESET_FILTER>();
    }
    else{
        throw std::logic_error("Invalid combination of arguments in Do_Filter");
    }
};

/// \brief Clones/constructs a new element of the same semantic kind under a new parent.
/// \param el Variant element to copy.
/// \param par New parent container.
/// \return New \c VariantElement owned by the caller.
/// \throws std::logic_error if the variant alternative is not handled.
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

/// \brief Looks up a UDT_RAW by type name.
/// \param type_to_search Key to find.
/// \param map UDT database.
/// \return UDT_RAW ptr or nullptr if not found.
std::shared_ptr<UDT_RAW> class_utils::lookup_udt(std::string type_to_search,UdtRawMap map) {
    if(auto it = map.find(type_to_search);it != map.end()){
        return it->second;
    }
    else{
        return nullptr;
    }
}

/// \brief Applies byte alignment padding to the given offset (even-byte alignment).
/// \param offset_in [in/out] Byte/bit offset to align; bit offset reset to 0.
void class_utils::apply_padding(std::pair<int,int>& offset_in){
    while(offset_in.first%2 !=0)
    {
        ++offset_in.first;
    }
    offset_in.second = 0;
};

/// \brief Ensures proper offset advance and alignment given a field size.
/// \param offset_in [in/out] Current byte/bit offset.
/// \param size Field size as {bytes,bits}.
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

/// \brief Resolves TIA basic type size from the lookup table.
/// \param type Type name (case-insensitive).
/// \return {bytes,bits}.
/// \throws std::logic_error if type is unknown.
std::pair<int,int> class_utils::get_size(std::string type){
    auto it = tia_type_size.find(to_lowercase(type));
    
    if(it == tia_type_size.end()){ 
        throw std::logic_error("Invalid element type");
    }
    return it->second;
};



/// \brief Extracts a boolean at byte/bit offset from a binary buffer.
/// \param buffer Source bytes.
/// \param byteOffset Byte offset.
/// \param bitOffset Bit index [0..7].
/// \return Bit value as bool.
bool translate::get_bool(const std::vector<unsigned char>& buffer, int byteOffset, int bitOffset) {

    return (buffer[byteOffset] >> bitOffset) & 0x01;
}

/// \brief Reads a big-endian integer of given length from buffer.
/// \param buffer Source bytes.
/// \param offset Start byte.
/// \param length Number of bytes.
/// \return Parsed integer (host int).
int translate::get_int(const std::vector<unsigned char>& buffer, int offset, int length) {
    int result = 0;
    for (int i = 0; i < length; ++i) {
        result |= (buffer[offset + i] << ((length - i - 1) * 8));
    }

    return result;
}

/// \brief Extracts a raw string from buffer.
/// \param buffer Source bytes.
/// \param offset Start byte.
/// \param length Number of bytes.
/// \return std::string constructed from the range.
/// \throws std::out_of_range if offset+length exceeds buffer size.
std::string translate::get_string(const std::vector<unsigned char>& buffer, int offset, int length) {
    if (offset + length > buffer.size()) {
        throw std::out_of_range("Buffer too small for string extraction");
    }

    return std::string(buffer.begin() + offset, buffer.begin() + offset + length);
}

/// \brief Generic typed read based on TIA type name.
/// \param buffer Source bytes.
/// \param offset_in {byte,bit} offset.
/// \param type_in TIA type (case-insensitive).
/// \return Value variant (bool/int/string) depending on type; 0 if unknown type.
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

/// \brief Parses a boolean string ("true"/"false").
/// \param bool_in Input string (lowercased expected).
/// \return Parsed bool (default false otherwise).
bool translate::parse_bool(std::string& bool_in)
{   
    if(bool_in == "true")
        return true;
    else
        return false;
}

/// \brief Parses a literal into a Value (int -> Value, "true/false" -> bool, otherwise string).
/// \param input Input lexeme (will be lowercased for bool test).
/// \return Parsed Value.
/// \note Falls back to returning the original string if not an int/bool.
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

/// \brief Sets the parent container pointer on this element.
/// \param in Parent container (shared).
void Element::set_parent(std::shared_ptr<BASE_CONTAINER> in ){parent = in;}

/// \brief Gets the parent container pointer.
/// \return Parent container (shared) or nullptr.
std::shared_ptr<BASE_CONTAINER> Element::get_parent()const {return parent;}

/// \brief BASE leaf constructor (name + type).
BASE::BASE(std::string name_in, std::string type_in) :
    name(name_in), type(type_in) {}

/// \brief Gets element name.
std::string BASE::get_name()const{return name;}

/// \brief Gets element type.
std::string BASE::get_type()const{return type;}

/// \brief Gets byte/bit offset of the element.
std::pair<int,int> BASE::get_offset() const { return offset;}

/// \brief Gets current decoded Value.
Value BASE::get_data() const{return data;}

/// \brief Gets current visibility flag.
bool BASE::get_vis() {return is_vis;}

/// \brief Sets element name.
void BASE::set_name(std::string name_in){ name = name_in;}

/// \brief Sets element type.
void BASE::set_type(std::string type_in){ type = type_in;}

/// \brief Decodes data from buffer according to type and this element's offset.
void BASE::set_data(const std::vector<unsigned char>& buffer){data = translate::generic_get(buffer,offset,type);};

/// \brief Sets visibility flag.
void BASE::set_vis(bool b_in) {is_vis = b_in;};

/// \brief Assigns offset to this element and advances a running offset cursor.
/// \param offset_in [in/out] Current {byte,bit} position advanced by this element size.
void BASE::set_offset(std::pair<int,int>& offset_in){
   std::pair<int,int> size = class_utils::get_size(type);

    class_utils::check_offset(offset_in,size);

    offset = offset_in;
    offset_in.first +=size.first;
    offset_in.second +=size.second;
};

/// \brief BASE_CONTAINER constructor (name + type).
BASE_CONTAINER::BASE_CONTAINER(std::string name_in,std::string type_in) 
    : name(name_in), type(type_in){}

/// \brief Gets container name.
std::string BASE_CONTAINER::get_name()const{return name;}

/// \brief Gets container type.
std::string BASE_CONTAINER::get_type()const{return type;}

/// \brief Gets children as variant list.
std::vector<VariantElement> BASE_CONTAINER::get_childs() const {return childs;}

/// \brief Gets name-to-element associations (if used).
std::vector<std::pair<std::string,VariantElement>> BASE_CONTAINER::get_names()const {return names;}

/// \brief Gets visibility flag.
bool BASE_CONTAINER::get_vis() {return is_vis;}

//// \brief Sets container name.
void BASE_CONTAINER::set_name(std::string name_in){name = std::move(name_in);}

/// \brief Sets container type.
void BASE_CONTAINER::set_type(std::string type_in){type = std::move(type_in);}

/// \brief Appends a child element to the container.
void BASE_CONTAINER::insert_child(VariantElement el){ childs.push_back(el);}

/// \brief Adds a named child association.
void BASE_CONTAINER::add_name(std::pair<std::string,VariantElement> el){names.push_back(el);}

/// \brief Sets visibility flag on container.
void BASE_CONTAINER::set_vis(bool b_in){ is_vis = b_in; };


/// \brief Propagates buffer decoding to leaf children (recursively for subcontainers).
/// \param buffer Source bytes from PLC DB.
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

/// \brief Assigns offsets to all children in order, updating a running cursor.
/// \param actual_offset [in/out] Accumulated byte/bit offset.
void BASE_CONTAINER::set_child_offset(std::pair<int,int>& actual_offset){
    for(auto& i : childs){
            check_type_for_offset(i,actual_offset);
        }
};

/// \brief Dispatches offset assignment by concrete child type, handling alignment for containers.
/// \param elem Child variant.
/// \param actual_offset [in/out] Accumulated byte/bit offset.
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

/// \brief UDT_RAW constructor holding the raw name only.
UDT_RAW::UDT_RAW(std::string name) :
    raw_name(name) {}

/// \brief Gets the raw UDT name.
std::string UDT_RAW::get_name() const { return raw_name;}

/// \brief Gets raw child elements of this UDT.
const std::vector<VariantElement>& UDT_RAW::get_childs() const { return childs;}

/// \brief Appends a child to the raw UDT.
void UDT_RAW::insert_child(VariantElement el_to_add){ childs.push_back(el_to_add);}

/// \brief Sets raw UDT name.
void UDT_RAW::set_name(std::string n_in){raw_name = std::move(n_in);}

/// \brief STD leaf constructor (name, type, parent).
STD_SINGLE::STD_SINGLE(std::string name_in,std::string type_in,std::shared_ptr<BASE_CONTAINER> parent)
    : BASE(std::move(name_in),std::move(type_in)) {this->set_parent(parent);}
    
/// \brief STD array element constructor (indexed leaf).
/// \param nr 0-based index within array range.
/// \param max Maximum index of the array (inclusive).
STD_ARR_ELEM::STD_ARR_ELEM(std::string name_in,std::string type_in,int nr,int max,std::shared_ptr<BASE_CONTAINER> parent)
        : STD_SINGLE(std::move(name_in),std::move(type_in),parent) , index(nr), max_index(max) {}

/// \brief Gets array element index.
int STD_ARR_ELEM::get_index() const { return index; }

/// \brief Gets max index for the array.
int STD_ARR_ELEM::get_max_index() const { return max_index; }

/// \brief Sets array element index.
void STD_ARR_ELEM::set_index(int index_in){index = index_in;}

/// \brief Sets max index for the array.
void STD_ARR_ELEM::set_max_index(int max_index){index = max_index;}

/// \brief STD array container constructor.
/// \param st Start index (inclusive).
/// \param end End index (inclusive).
STD_ARRAY::STD_ARRAY(const std::string& n_in,const std::string& t_in,int st,int end,std::shared_ptr<BASE_CONTAINER> parent)
    : BASE_CONTAINER(n_in,t_in),index_start(st),index_end(end){this->set_parent(parent);}

/// \brief Factory to create a STD array container with indexed children.
/// \throws std::logic_error if element type is unknown in \c tia_type_size.
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

/// \brief UDT container (single instance) constructor.
UDT_SINGLE::UDT_SINGLE(std::string name_in ,std::string type_in)
    :BASE_CONTAINER(name_in,type_in){}

/// \brief Factory to create a UDT_SINGLE from a raw element template.
/// \param el Raw children to clone under the new instance.
std::shared_ptr<UDT_SINGLE> UDT_SINGLE::create_from_element
    (std::string n_in,std::string t_in,const std::vector<VariantElement>& el,std::shared_ptr<BASE_CONTAINER> parent)
    {
        std::shared_ptr<UDT_SINGLE> self = std::make_shared<UDT_SINGLE>(n_in,t_in);
        self->set_parent(parent);
        ElementInfo info;
        info.is_arr = false; // =^.^=
        for(auto i : el){
            auto new_el = class_utils::create_element(i,self);
            std::visit([&](auto&& ptr){ ptr->set_parent(self);},new_el);
            self->insert_child(new_el);
        }
        return self;          
    }

/// \brief UDT array element copy-constructor from existing UDT_ARR_ELEM.
/// \param el Source element to copy structure from.
/// \param par New parent.
UDT_ARR_ELEM::UDT_ARR_ELEM(std::shared_ptr<UDT_ARR_ELEM> el,std::shared_ptr<BASE_CONTAINER> par)
    : UDT_SINGLE(el->get_name(),el->get_type()) 
    {
        index = el->get_index();
        max_index = el->get_max_index();
        childs = el->get_childs();
        parent = par;
    }

/// \brief Factory to create a UDT_ARR_ELEM from a UDT_SINGLE base template.
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

/// \brief UDT array copy-constructor from an existing array instance.
/// \param el Source UDT_ARRAY to clone.
/// \param par New parent.
UDT_ARRAY::UDT_ARRAY(std::shared_ptr<UDT_ARRAY> el,std::shared_ptr<BASE_CONTAINER> par)
    : BASE_CONTAINER(el->get_name(),el->get_type()) 
    {
        index_start = el->get_start();
        index_end = el->get_end();
        childs = el->get_childs();
        parent = par;
    }
    
/// \brief Gets UDT array name.
std::string UDT_ARRAY::get_name()const{return name;};

/// \brief Gets UDT array type.
std::string UDT_ARRAY::get_type()const{return type;}

/// \brief Gets start index.
int UDT_ARRAY::get_start()const{return index_start;}

/// \brief Gets end index.
int UDT_ARRAY::get_end()const{return index_end;}

/// \brief Factory to create a UDT_ARRAY by replicating a UDT_SINGLE template.
/// \param el Raw children used to build the base template instance.
/// \param start First index (inclusive).
/// \param end Last index (inclusive).
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

/// \brief STRUCT single container constructor.
STRUCT_SINGLE::STRUCT_SINGLE( std::string& name_in,std::string type_in) 
    : BASE_CONTAINER(name_in,"Struct"){}

/// \brief Gets struct display name.
std::string STRUCT_SINGLE::get_name() const { return name; }

/// \brief Gets struct display type (always "Struct").
std::string STRUCT_SINGLE::get_type() const { return "Struct";}
    
/// \brief Factory to create a STRUCT_SINGLE from raw children.
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

/// \brief STRUCT array element constructor (indexed struct).
STRUCT_ARRAY_EL::STRUCT_ARRAY_EL(std::string& n_in,int idx,int idx_max,std::string t_in) 
    : STRUCT_SINGLE(n_in,"Struct"),index(idx),max_index(idx_max){}

/// \brief Default constructor (for factory use).
STRUCT_ARRAY_EL::STRUCT_ARRAY_EL() = default;

/// \brief Gets struct array element index.
int STRUCT_ARRAY_EL::get_index()const{return index;}

/// \brief Gets struct array max index.
int STRUCT_ARRAY_EL::get_max_index()const{return max_index;}

/// \brief Sets struct array element index.
void STRUCT_ARRAY_EL::set_index(int i){index = i;}

/// \brief Sets struct array max index.
void STRUCT_ARRAY_EL::set_max_index(int i){max_index = i;}

/// \brief Factory to create a STRUCT_ARRAY_EL from raw children.
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

/// \brief STRUCT array container constructor.
STRUCT_ARRAY::STRUCT_ARRAY( std::string& name_in,std::string type_in) 
    : BASE_CONTAINER(name_in,"Struct"){}

/// \brief Gets struct array display name.
std::string STRUCT_ARRAY::get_name() const { return name; }

/// \brief Gets struct array display type (always "Struct").
std::string STRUCT_ARRAY::get_type() const { return "Struct";}

/// \brief Gets array start index (inclusive).
int STRUCT_ARRAY::get_start()const{return start;}

/// \brief Gets array end index (inclusive).
int STRUCT_ARRAY::get_end()const{return end;}

/// \brief Factory to create a STRUCT_ARRAY by replicating a STRUCT template across indices.
/// \warning The current implementation constructs children under \c self inside the loop
/// but does not initialize \c self before usage; ensure \c self is created and parented first.
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

/// \brief DB container constructor.
DB::DB(std::string name_in) :BASE_CONTAINER(name_in,"DB"){};

/// \brief Gets DB name.
std::string DB::get_name() const{return name;}

/// \brief Gets maximum computed offset after layout.
std::pair<int,int> DB::get_max_offset()const{return offset_max;}

/// \brief Sets maximum offset.
void DB::set_max_offset(std::pair<int,int> ofst){offset_max = ofst;}

/// \brief Computes children offsets starting from current max offset.
void DB::_set_offset(){set_child_offset(offset_max);}

/// \brief Propagates buffer decoding to the entire DB.
void DB::_set_data(const std::vector<unsigned char>& buffer){set_data_to_child(buffer);}


