#include <parser.hpp>

/// \brief Root rule action: complete datablock parsed.
/// \details Currently a no-op; hook for top-level file handling/logging.
template<>
struct action<complete_datablock> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
       //std::cout << "complete datablock found" <<",\n";
    }
};

/// \brief UDT header: captures quoted UDT name and pushes a new UDT_RAW on the scope stack.
/// \details Starts a new UDT context; children parsed next will be added under this node.
template<>
struct action<udt_header_quoted_name> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        st.element_in_scope.push_back(std::make_shared<UDT_RAW>(in.string()));
        //std::cout << "UDT QUOTED NAME: "<< in.string()<<"\n";
    }
};       
   
/// \brief UDT field name within a STRUCT body.
/// \details Ignores sentinel tokens like END_TYPE; records the field name in ParserState.
template<>
struct action<udt_raw_basic_name> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        //std::cout << "UDT RAW BASIC: "<< in.string()<<"\n";
        if(in.string() != "END_TYPE"){
            st.name = in.string();
        }
    }
};

/// \brief UDT field type (or STRUCT opener).
/// \details Sets \c st.type. If a STRUCT is detected, creates a STRUCT_SINGLE, assigns parent,
/// and pushes it on the scope stack so subsequent fields attach to it.
template<>
struct action<udt_raw_basic_type> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        //std::cout << "UDT RAW BASIC TYPE: "<< in.string()<<"\n";
        
        st.type = in.string();
        if(st.type.find("Struct") != std::string::npos) {
            std::shared_ptr<STRUCT_SINGLE> el = std::make_shared<STRUCT_SINGLE>(st.name,"Struct");
        std::shared_ptr<BASE_CONTAINER> par;
        std::visit([&](auto&& el)
        {
            using E = std::decay_t<decltype(el)>;
            if constexpr(std::is_same_v<E,std::shared_ptr<DB>>) par = el;
            else par = nullptr;

        },st.element_in_scope[0]);
            el->set_parent(par);
            st.element_in_scope.push_back(el);//=^.^=
        }
    }
};
            
/// \brief Array start index for UDT field.
/// \details Sets \c st.array_start and marks \c st.is_arr = true.
template<>
struct action<udt_raw_basic_array_start_arr> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        //std::cout << "UDT RAW BASIC ARRAY START ARR: "<< in.string()<<"\n";
        st.array_start = std::stoi(in.string());
        st.is_arr = true;
    }
};
            
/// \brief Array end index for UDT field.
template<>
struct action<udt_raw_basic_array_end_arr> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        //std::cout << "UDT  RAW BASIC END ARR: "<< in.string()<<"\n";
        st.array_end = std::stoi(in.string());
    }
};
            
/// \brief End of declaration (‘;’) inside UDT/DB field lists.
/// \details Materializes the current (name/type/array) element into the active scope.
template<>
struct action<end_declaration> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        //std::cout << "END DECLARATION: "<< in.string()<<"\n";
        st.insert_element_inscope();
    };
};

/// \brief End of UDT STRUCT body (`END_STRUCT;`).
/// \details Pops the current STRUCT scope; if a UDT_RAW or DB is the parent, inserts the struct as child.
/// If this is the top UDT on the stack, registers it in the UDT database.
template<>
struct action<udt_raw_basic_struct_end> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        //std::cout << "UDT RAW BASIC STRUCT END: "<< in.string()<<"\n";
        ScopeVariant el_to_insert = st.element_in_scope.back();
        if (st.element_in_scope.size() > 1) {
            ScopeVariant el = st.element_in_scope[st.element_in_scope.size() - 2];
            std::visit([&](auto&& parent_ptr) {
                std::visit([&](auto&& child_ptr) {
                    using ParentT = std::decay_t<decltype(parent_ptr)>;
                    using ChildT = std::decay_t<decltype(child_ptr)>;

                    if constexpr ((std::is_same_v<ParentT, std::shared_ptr<UDT_RAW>> ||
                        std::is_same_v<ParentT, std::shared_ptr<DB>>) &&
                        std::is_same_v<ChildT, std::shared_ptr<STRUCT_SINGLE>>) {
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

/// \brief UDT terminator (`END_TYPE`).
/// \details Currently a no-op (logging hook).
template<>
struct action<udt_header_end_declaration> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        //std::cout << "UDT END DECLARATION: "<< in.string()<<"\n";
    }
};

/// \brief DB header start (`DATA_BLOCK ...`).
/// \details Creates the DB root using \c st.DB_name and pushes it on the scope stack.
template<>
struct action<db_header_declaration> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        //std::cout << "DB HEADER DECLARATION: "<< in.string()<<"\n";
        st.db = std::make_shared<DB>(st.DB_name);
        st.element_in_scope.push_back(st.db);
    }
};

/// \brief DB name token (header).
/// \details Currently a no-op; kept for completeness/logging.
template<>
struct action<db_header_name> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        //std::cout << "DB HEADER NAME: "<< in.string()<<"\n";
    }
};

/// \brief DB body field name.
/// \details Stores the field name (ignores END_STRUCT sentinel).
template<>
struct action<db_body_name> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        //std::cout << "DB BODY NAME: "<< in.string()<<"\n";
        if(in.string() != "END_STRUCT")
        st.name = in.string();
    }
};

/// \brief DB array start index for a field.
template<>
struct action<db_body_array_start> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        //std::cout << "DB BODY ARRAY START: "<< in.string()<<"\n";
        st.is_arr = true;
        st.array_start = std::stoi(in.string());
    }
};

/// \brief DB array end index for a field.
template<>
struct action<db_body_array_end> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        //std::cout << "DB BODY ARRAY END: "<< in.string()<<"\n";
        st.array_end = std::stoi(in.string());
    }
};

/// \brief DB body field type (or STRUCT opener).
/// \details Sets \c st.type. If a STRUCT is detected, pushes a new STRUCT_SINGLE on the scope stack.
template<>
struct action<db_body_type> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        //std::cout << "DB BODY TYPE: "<< in.string()<<"\n";
        st.type = in.string();
        if(st.type.find("Struct") != std::string::npos) {
            st.element_in_scope.push_back(std::make_shared<STRUCT_SINGLE>(st.name,"Struct"));
        }
    }
};


/// \brief DB body inclusion from a UDT reference (single token line).
/// \details Uses the same token as name and type, resolves the UDT in \c st.udt_database,
/// and emits the corresponding node via \c st.insert_element_inscope().
template<>
struct action<db_from_udt> {
    template<typename Input>
    static void apply(const Input& in, ParserState& st) {
        //std::cout << "DB FROM UDT: "<< in.string()<<"\n"<<"\n";
        st.name = in.string();
        st.type = in.string();
        if(auto it = st.udt_database.find(st.type);it != st.udt_database.end())
        {
            st.insert_element_inscope();
        }
        
    }
};
