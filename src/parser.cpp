#include <parser.hpp>
#include <classes.hpp>

/// \brief Resets the transient parse state for the current field being built.
/// \details Clears name and type, resets array flags and bounds. Should be called
/// after emitting a field (STD/UDT, single/array) into the current scope.
void ParserState::clear_state(){
    name.clear();
    type.clear();
    is_arr = false;
    array_start = 0;
    array_end = 0;
};

/// \brief Looks up a UDT definition by \c type in the UDT database.
/// \return Shared pointer to the matching \c UDT_RAW, or \c nullptr if not found.
/// \note The lookup key is \c type as captured from the grammar; callers should ensure
/// it matches how UDT names are stored (quoted vs unquoted normalization).
std::shared_ptr<UDT_RAW> ParserState::lookup_udt() const {
    if(auto it = udt_database.find(type);it != udt_database.end()){
        return it->second;
    }
    else{
        //std::cout << "Error on UDT Database Creation"<< "\n";
        return nullptr;
    }
};

/// \brief Materializes the currently parsed element into the active parent scope.
/// \details Builds either STD/UDT, single/array nodes based on the transient state
/// (name, type, is_arr, array_start/end) and inserts the created child into the
/// container at the top of \c element_in_scope.
/// 
/// Logic:
///  - Determines the parent container (also storing a raw parent pointer for back-refs).
///  - If \c type contains a quotation mark it is treated as a UDT reference:
///    - Resolves the UDT via \c lookup_udt().
///    - Creates \c UDT_SINGLE or \c UDT_ARRAY and inserts it into the current scope.
///  - Otherwise treats \c type as a standard type:
///    - Creates \c STD_SINGLE or \c STD_ARRAY and inserts it.
///  - Finally calls \c clear_state() to reset the transient fields.
/// 
/// \note The parent pointer \c par is inferred by visiting \c element_in_scope[0]
///       and setting it only when the root is a \c std::shared_ptr<DB>. This assumes
///       the first element of the scope stack is the DB root. If nested containers
///       require accurate parent back-references, consider deriving \c par from
///       the actual current container instead of the root.
/// \warning If the UDT is not found, an error is logged and nothing is inserted.
/// \remark The test for UDT vs STD uses \c type.find('"') to detect quoted names.
///         If your grammar normalizes names, prefer an explicit flag set by the parser.
void ParserState::insert_element_inscope(){
    ScopeVariant element = element_in_scope.back();
    std::shared_ptr<BASE_CONTAINER> par;
    std::visit([&](auto&& el)
    {
        using E = std::decay_t<decltype(el)>;
        if constexpr(std::is_same_v<E,std::shared_ptr<DB>>) par = el;
        else par = nullptr;

    },element_in_scope[0]);

    if(type.find('"') != std::string::npos){

        auto it = lookup_udt();
        if (it != nullptr) {
            if(!is_arr){
                std::shared_ptr<UDT_SINGLE> el = UDT_SINGLE::create_from_element(name,it->get_name(),it->get_childs(),par);
                
                std::visit([&](auto&& ptr) {
                    ptr->insert_child(el);
                },element);
            }
            else{
                std::shared_ptr<UDT_ARRAY> el = UDT_ARRAY::create_from_element(name,it->get_name(),it->get_childs(),array_start,array_end,par);
                
                std::visit([&](auto&& ptr) {
                    ptr->insert_child(el);
                },element);
            }
        } else {
            std::cerr << "Erorr, UDT " << type << " not found\n";
        }
    }
    else{
        if(!is_arr){
            std::visit([&](auto&& ptr) {
                std::shared_ptr<STD_SINGLE> el = std::make_shared<STD_SINGLE>(name,type,par);
                ptr->insert_child(el);
            },element);
        }
        else{
            std::visit([&](auto&& ptr) {
                std::shared_ptr<STD_ARRAY> el = STD_ARRAY::create_array(name,type,array_start,array_end,par);
                ptr->insert_child(el);
            },element);
        }
    }
    
    clear_state();
} 
