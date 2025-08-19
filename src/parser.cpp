#include <parser.hpp>
#include <classes.hpp>

void ParserState::clear_state(){
    name.clear();
    type.clear();
    is_arr = false;
    array_start = 0;
    array_end = 0;
};

std::shared_ptr<UDT_RAW> ParserState::lookup_udt() const {
    if(auto it = udt_database.find(type);it != udt_database.end()){
        return it->second;
    }
    else{
        //std::cout << "Error on UDT Database Creation"<< "\n";
        return nullptr;
    }
};

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
