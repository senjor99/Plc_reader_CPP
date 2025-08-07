#pragma once

#include <datatype.hpp>


class UDT_RAW {
    protected:
    std::string raw_name;
    std::vector<VariantElement> childs;

    public:
    UDT_RAW() = default;
    UDT_RAW(std::string name) : raw_name(name) {}

    std::string get_name() const {return raw_name;}
    const auto& get_childs() const {return childs;}

    void insert_child(VariantElement element_to_add) { childs.push_back(element_to_add);}
    void set_name(std::string name_in){raw_name = std::move(name_in);}
};

class STD_SINGLE : public BASE {
    public:
    STD_SINGLE() = default;
    STD_SINGLE(std::string name_in,std::string type_in,std::shared_ptr<BASE_CONTAINER> parent)
        : BASE(std::move(name_in),std::move(type_in)) {this->set_parent(parent);}
};

class STD_ARR_ELEM : public STD_SINGLE {
    protected:
    int index;
    int max_index;

    public:
    int get_index() const { return index; }
    int get_max_index() const { return max_index; }
    
    void set_index(int index_in){index = index_in;}
    void set_max_index(int max_index){index = max_index;}

    STD_ARR_ELEM()=default;
    STD_ARR_ELEM(std::string name_in,std::string type_in,int nr,int max,std::shared_ptr<BASE_CONTAINER> parent)
        : STD_SINGLE(std::move(name_in),std::move(type_in),parent) , index(nr), max_index(max) {}

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
    STD_ARRAY
    (  
        const std::string& name_in,
        const std::string& type_in,
        int start,
        int end,std::shared_ptr<BASE_CONTAINER> parent
    ) 
        : BASE_CONTAINER(std::move(name_in),std::move(type_in)),index_start(start),index_end(end){this->set_parent(parent);}

    static std::shared_ptr<STD_ARRAY> create_array
    (
        const std::string& name_in,
        const std::string& type_in,
        int start,
        int end,
        std::shared_ptr<BASE_CONTAINER> parent
    ) 
    {   
        auto arr = std::make_shared<STD_ARRAY>();
        arr->name = (name_in);
        arr->type = type_in;
        arr->index_start = start ;
        arr->index_end = end;
        
        auto it = type_size.find(to_lowercase(type_in));
        if(it == type_size.end()){ 
            std::cout <<"element name "<< to_lowercase(type_in)<< "\n";
            throw std::logic_error("Invalid element type");
        }
      
        for (int i = start; i <= end; ++i) {
            std::string indexed_name = name_in + "[" + std::to_string(i) + "]";
            VariantElement el = std::make_shared<STD_ARR_ELEM>(indexed_name, type_in, i,end,arr);
            arr->childs.push_back(el);
        }
        return arr;
    }

};


class UDT_SINGLE : public BASE_CONTAINER  {
    protected:
    // CLASS ATTRIBUTE
    std::vector<std::string> contained_names;
    bool is_enabled;
    
    public:
    UDT_SINGLE()=default;
    UDT_SINGLE(std::string name_in ,std::string type_in)
        :BASE_CONTAINER(std::move(name_in),std::move(type_in)){}

    static std::shared_ptr<UDT_SINGLE> create_from_element
    (
        std::string name_in,
        std::string type_in,
        const std::vector<VariantElement>& el,
        std::shared_ptr<BASE_CONTAINER> parent
    )
    {
        std::shared_ptr<UDT_SINGLE> self = std::make_shared<UDT_SINGLE>(name_in,type_in);
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
};

class UDT_ARR_ELEM : public UDT_SINGLE {
    protected:
    int index;
    int max_index;

    public:
    int get_index()const{return index; }
    int get_max_index()const{return max_index;}
    UDT_ARR_ELEM()=default;
    UDT_ARR_ELEM(std::shared_ptr<UDT_ARR_ELEM> el,std::shared_ptr<BASE_CONTAINER> par)
        : UDT_SINGLE(std::move(el->get_name()),std::move(el->get_type())) {
            index = el->get_index();
            max_index = el->get_max_index();
            childs = el->get_childs();
            parent = par;
        }

    static std::shared_ptr<UDT_ARR_ELEM> create_from_element
    (
        std::shared_ptr<UDT_SINGLE> el,
        int arr_nr,
        int max_arr_nr,
        std::shared_ptr<BASE_CONTAINER> parent
    )
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
   

};

class UDT_ARRAY : public BASE_CONTAINER {
    protected:
    int index_start;
    int index_end;

    public:
    std::string get_type()const{return type;}
    int get_start()const{return index_start;}
    int get_end()const{return index_end;}
    UDT_ARRAY() = default;

    UDT_ARRAY(std::shared_ptr<UDT_ARRAY> el,std::shared_ptr<BASE_CONTAINER> par)
        : BASE_CONTAINER(std::move(el->get_name()),std::move(el->get_type())) {
            index_start = el->get_start();
            index_end = el->get_end();
            childs = el->get_childs();
            parent = par;
        }
    
    std::string get_name() const {return name;}

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
};

class STRUCT_SINGLE  : public BASE_CONTAINER {
    protected:

    public:
    STRUCT_SINGLE()=default;
    STRUCT_SINGLE( std::string& name_in,std::string type_in="Struct") 
        : BASE_CONTAINER(std::move(name_in),std::move(type_in)){}
    
    std::string get_name() const { return name; }
    std::string get_type() const { return "Struct";}


    static std::shared_ptr<STRUCT_SINGLE> create_from_element
    (
        std::string name_in,
        std::string type_in,
        const std::vector<VariantElement> el,
        std::shared_ptr<BASE_CONTAINER> parent
    )
    {
        std::shared_ptr<STRUCT_SINGLE> self = std::make_shared<STRUCT_SINGLE>(name_in,type_in);
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
};

class DB : public BASE_CONTAINER{
    protected:
    int default_nr;
    
    public:
    std::pair<int,int> offset_max ; 

    std::string get_name() const { return name;}
    
    DB() = default;
    DB(std::string name_in) :BASE_CONTAINER(std::move(name_in),"DB"){};
    void _set_offset(){set_child_offset(offset_max);}
    void _set_data(const std::vector<unsigned char>& buffer){set_data_to_child(buffer);}
    };

