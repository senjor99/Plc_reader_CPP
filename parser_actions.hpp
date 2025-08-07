#pragma once

#include <parser.hpp>


struct ParserState {
    
    UdtRawMap udt_database;
    // Parser scope Elements:
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
    
    void clear_state(){
        name.clear();
        type.clear();
        is_arr = false;
        array_start = 0;
        array_end = 0;
    }

    std::shared_ptr<UDT_RAW> lookup_udt() const {
        if(auto it = udt_database.find(type);it != udt_database.end()){
            return it->second;
        }
        else{
            //std::cout << "Error on UDT Database Creation"<< "\n";
            return nullptr;
        }
    }

    void insert_element_inscope(){
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
};

template<typename Rule>
struct action {
    template<typename Input>
    static void apply(const Input&, DB&,ParserState&) {
    }
};

//
template<>
struct action<complete_datablock> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
       //std::cout << "complete datablock found" <<",\n";
    }
};



// Start of the declaration of a new UDT
// and put the scope onto an UDT
// new incoming data  will fullfill the information needed
// to create the UDT

template<>
struct action<udt_header_quoted_name> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
       //std::cout << " UDT name: " << in.string() <<",\n";
        st.element_in_scope.push_back(std::make_shared<UDT_RAW>(in.string()));
    }
};       
   
template<>
struct action<udt_raw_basic_name> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        if(in.string() != "END_TYPE"){
            st.name = in.string();
        }
    }
};

template<>
struct action<udt_raw_basic_type> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
       //std::cout << " Element is type of: " << in.string() <<",\n";
        st.type = in.string();
        if(st.type.find("Struct") != std::string::npos) {
            std::shared_ptr<STRUCT_SINGLE> el = std::make_shared<STRUCT_SINGLE>(st.name);
        std::shared_ptr<BASE_CONTAINER> par;
        std::visit([&](auto&& el)
        {
            using E = std::decay_t<decltype(el)>;
            if constexpr(std::is_same_v<E,std::shared_ptr<DB>>) par = el;
            else par = nullptr;

        },st.element_in_scope[0]);
            el->set_parent(par);
            st.element_in_scope.push_back(el);
        }
    }
};
            
template<>
struct action<udt_raw_basic_array_start_arr> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        //std::cout << " Array start at " << in.string() <<",\n";
        st.array_start = std::stoi(in.string());
        st.is_arr = true;
    }
};
            
template<>
struct action<udt_raw_basic_array_end_arr> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
       //std::cout << " Array ends at " << in.string() <<",\n";
        st.array_end = std::stoi(in.string());
    }
};
            
template<>
struct action<end_declaration> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        
        st.insert_element_inscope();
    };
};
template<>
struct action<udt_raw_basic_struct_end> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        //std::cout<< "inserting in scope, pointer: "<<st.element_in_scope.size();
        ScopeVariant el_to_insert = st.element_in_scope.back();
        if (st.element_in_scope.size() > 1) {
            ScopeVariant el = st.element_in_scope[st.element_in_scope.size() - 2];
            //std::cout<<"test \n";
            std::visit([&](auto&& parent_ptr) {
                std::visit([&](auto&& child_ptr) {
                    using ParentT = std::decay_t<decltype(parent_ptr)>;
                    using ChildT = std::decay_t<decltype(child_ptr)>;

                    if constexpr ((std::is_same_v<ParentT, std::shared_ptr<UDT_RAW>> ||
                        std::is_same_v<ParentT, std::shared_ptr<DB>>) &&
                        std::is_same_v<ChildT, std::shared_ptr<STRUCT_SINGLE>>) {
                        //std::cout<<"inserendo struct "<<child_ptr->get_name()<< " in " << parent_ptr->get_name();
                            parent_ptr->insert_child(child_ptr);
                    }
                }, el_to_insert);
            }, el);
        } else {
            std::visit([&](auto&& ptr){
                using T = std::decay_t<decltype(ptr)>;
                if constexpr (std::is_same_v<T, std::shared_ptr<UDT_RAW>>){
                    st.udt_database[ptr->get_name()] = ptr;
                }
            },el_to_insert);
        }
        st.element_in_scope.pop_back();
    }
};

template<>
struct action<udt_header_end_declaration> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
       //std::cout <<"endtype "<<in.string() <<"\n";
    }
};

template<>
struct action<db_header_declaration> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
       //std::cout <<"db title "<<in.string() <<",\n";
        st.db = std::make_shared<DB>(st.DB_name);
        st.element_in_scope.push_back(st.db);
    }
};

template<>
struct action<db_header_name> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
       //std::cout <<"db header "<<in.string() <<",\n";
    }
};

template<>
struct action<db_body_name> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        if(in.string() != "END_STRUCT")
       //std::cout <<" DB Element Name: "<<in.string() <<",\n";
        st.name = in.string();
    }
};

template<>
struct action<db_body_array_start> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
       //std::cout <<" DB Element Arr Start:  "<<in.string() <<",\n";
        st.is_arr = true;
        st.array_start = std::stoi(in.string());
    }
};

template<>
struct action<db_body_array_end> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
       //std::cout <<" DB Element Arr End:  "<<in.string() <<",\n";
        st.array_end = std::stoi(in.string());
    }
};

template<>
struct action<db_body_type> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
       //std::cout <<" DB Element Type:  "<<in.string() <<",\n";
        st.type = in.string();
        if(st.type.find("Struct") != std::string::npos) {
            st.element_in_scope.push_back(std::make_shared<STRUCT_SINGLE>(st.name));
        }
    }
};


template<>
struct action<db_from_udt> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        //std::cout <<" DB Element Type:  "<<in.string() <<",\n";
        st.name = in.string();
        st.type = in.string();
        if(auto it = st.udt_database.find(st.type);it != st.udt_database.end())
        {
            st.insert_element_inscope();
        }
        
    }
};
