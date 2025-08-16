#pragma once

#include <datatype.hpp>


class Element{
    protected:
        bool is_vis = true;
        std::shared_ptr<BASE_CONTAINER> parent;
    public:
        void set_parent(std::shared_ptr<BASE_CONTAINER> in );
        virtual void set_vis(bool in) = 0;

        std::shared_ptr<BASE_CONTAINER> get_parent()const;
        virtual bool get_vis() =0;
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
    BASE(std::string name_in, std::string type_in);
    
    std::string get_name() const;
    std::string get_type() const;
    std::pair<int,int> get_offset() const;
    Value get_data()const;
    
    void set_name(std::string name_in);
    void set_type(std::string type_in);
    void set_data(const std::vector<unsigned char>& buffer);
    void set_vis(bool b_in);
    void set_offset(std::pair<int,int>& offset_in);
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
    BASE_CONTAINER(std::string name_in,std::string type_in);
    
    std::string get_name()const;
    std::string get_type()const;
    std::vector<VariantElement> get_childs()const;
    std::vector<std::pair<std::string,VariantElement>> get_names()const;
    
    void set_name(std::string name_in);
    void set_type(std::string type_in);
    void insert_child(VariantElement el);
    void add_name(std::pair<std::string,VariantElement> el);
    void set_vis(bool b_in) override;
    
    void set_data_to_child(const std::vector<unsigned char>& buffer);
    void set_child_offset(std::pair<int,int>& actual_offset);
    void check_type_for_offset(VariantElement& elem,std::pair<int,int>& actual_offset);
    
};

class UDT_RAW {
    protected:
    std::string raw_name;
    std::vector<VariantElement> childs;

    public:
    UDT_RAW() = default;
    UDT_RAW(std::string name);

    std::string get_name()const;
    const auto& get_childs() const;

    void insert_child(VariantElement el_to_add);
    void set_name(std::string n_in);
};

class STD_SINGLE : public BASE {
    public:
    STD_SINGLE() = default;
    STD_SINGLE(std::string name_in,std::string type_in,std::shared_ptr<BASE_CONTAINER> parent);
};

class STD_ARR_ELEM : public STD_SINGLE {
    protected:
    int index;
    int max_index;

    public:
    STD_ARR_ELEM()=default;
    STD_ARR_ELEM(std::string name_in,std::string type_in,int nr,int max,std::shared_ptr<BASE_CONTAINER> parent);

    int get_index() const;
    int get_max_index() const;
    
    void set_index(int index_in);
    void set_max_index(int max_index);
};

class STD_ARRAY : public BASE_CONTAINER {
    protected:
    int index_start;
    int index_end;
    
    public:
    int get_start()const{return index_start;}
    int get_end()const{return index_end;}

    void set_start(int start_in){index_start =start_in; }
    void set_end(int end_in){index_end = end_in;}

    STD_ARRAY()=default;
    STD_ARRAY(const std::string& n_in,const std::string& t_in,int st,int end,std::shared_ptr<BASE_CONTAINER> parent);
    

    static std::shared_ptr<STD_ARRAY> create_array
        (const std::string& n_in,const std::string& t_in,int st,int end,std::shared_ptr<BASE_CONTAINER> parent);
};


class UDT_SINGLE : public BASE_CONTAINER  {
    protected:
    // CLASS ATTRIBUTE
    std::vector<std::string> contained_names;
    bool is_enabled;
    
    public:
    UDT_SINGLE()=default;
    UDT_SINGLE(std::string name_in ,std::string type_in);

    static std::shared_ptr<UDT_SINGLE> create_from_element
        (std::string n_in,std::string t_in,const std::vector<VariantElement>& el,std::shared_ptr<BASE_CONTAINER> parent);
};

class UDT_ARR_ELEM : public UDT_SINGLE {
    protected:
    int index;
    int max_index;

    public:
    int get_index()const{return index; }
    int get_max_index()const{return max_index;}
    UDT_ARR_ELEM()=default;
    UDT_ARR_ELEM(std::shared_ptr<UDT_ARR_ELEM> el,std::shared_ptr<BASE_CONTAINER> par);

    static std::shared_ptr<UDT_ARR_ELEM> create_from_element
        (std::shared_ptr<UDT_SINGLE> el,int arr_nr,int max_arr_nr,std::shared_ptr<BASE_CONTAINER> parent);
};

class UDT_ARRAY : public BASE_CONTAINER {
    protected:
    int index_start;
    int index_end;

    public:
    UDT_ARRAY() = default;
    UDT_ARRAY(std::shared_ptr<UDT_ARRAY> el,std::shared_ptr<BASE_CONTAINER> par);

    std::string get_name() const;
    std::string get_type()const;
    int get_start()const;
    int get_end()const;
    

    static std::shared_ptr<UDT_ARRAY> create_from_element
    (
        std::string name_in,
        std::string type_in,
        const std::vector<VariantElement> el,
        int start,
        int end,
        std::shared_ptr<BASE_CONTAINER> parent
    );

};

class STRUCT_SINGLE  : public BASE_CONTAINER {
    public:
    STRUCT_SINGLE()=default;
    STRUCT_SINGLE( std::string& name_in,std::string type_in="Struct");
    
    std::string get_name()const;
    std::string get_type()const;


    static std::shared_ptr<STRUCT_SINGLE> create_from_element
        (std::string n_in,std::string t_in,const std::vector<VariantElement> el,std::shared_ptr<BASE_CONTAINER> parent);
};

class STRUCT_ARRAY_EL : public STRUCT_SINGLE {
    protected:
    int index;
    int max_index;

    public:
    STRUCT_ARRAY_EL() = default;
    STRUCT_ARRAY_EL(std::string& n_in,int idx,int idx_max,std::string t_in="Struct") ;

    int get_index()const;
    int get_max_index()const;

    void set_index(int i);
    void set_max_index(int i);

    static std::shared_ptr<STRUCT_ARRAY_EL> create_from_element
    (
        std::string name_in,
        int idx,
        int idx_max,
        const std::vector<VariantElement> el,
        std::shared_ptr<BASE_CONTAINER> parent
    );
};

class STRUCT_ARRAY  : public BASE_CONTAINER {
    protected:

    public:
    STRUCT_ARRAY()=default;
    STRUCT_ARRAY( std::string& name_in,std::string type_in="Struct") ;
    
    std::string get_name() const;
    std::string get_type() const;


    static std::shared_ptr<STRUCT_ARRAY> create_from_element
    (
        std::string name_in,
        std::string type_in,
        const std::vector<VariantElement> el,
        std::shared_ptr<BASE_CONTAINER> parent,
        int start,
        int end
    );
};
class DB : public BASE_CONTAINER{
    protected:
    int default_nr;
    std::pair<int,int> offset_max ; 
    
    public:
    DB() = default;
    DB(std::string name_in);

    std::string get_name() const;
    std::pair<int,int> get_max_offset()const;

    void _set_offset();
    void set_max_offset(std::pair<int,int> ofst);
    void _set_data(const std::vector<unsigned char>& buffer);
    };

