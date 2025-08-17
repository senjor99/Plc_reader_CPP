#pragma once

#include <tao/pegtl.hpp>
#include <tao/pegtl/utf8.hpp>
#include <tao/pegtl/contrib/parse_tree.hpp>
#include "datatype.hpp"

namespace pt = tao::pegtl;


//::::::::::::::PARSER GENERAL:::::::::::::://
//::::::::::::::BASIC ELEMENT::::::::::::::://
//:::::::::::::::::::::::::::::::::::::::::://
using space = pt::space ;
struct indent : pt::plus<space>{};
struct separator : pt::seq<space,pt::one<':'>,space> {};
struct end_declaration : pt::one<';'> {};
struct attribute : pt::seq<pt::opt<space>, pt::if_must<pt::one<'{'>, pt::until<pt::one<'}'>>> >{};
struct comment : pt::if_must<pt::three<' '>,TAO_PEGTL_STRING("//"),pt::until<pt::eol>>{};
struct start_value :pt::seq<space, pt::if_must<TAO_PEGTL_STRING(":="),pt::until<end_declaration>>> {};
struct version : pt::seq<TAO_PEGTL_STRING("VERSION"),separator,pt::plus<pt::digit>,pt::one<'.'>,pt::plus<pt::digit>,pt::eol>{};
struct version_noeol : pt::seq<TAO_PEGTL_STRING("VERSION"),separator,pt::plus<pt::digit>,pt::one<'.'>,pt::plus<pt::digit>>{};
struct alphanum : pt::plus<pt::sor<pt::alnum,pt::one<'_'>,pt::one<'-'>,pt::one<'.'>,pt::one<'/'>>> {}; 
struct alphanum_space : pt::plus<pt::sor<pt::alnum,pt::one<'_'>,space,pt::one<'-'>,pt::one<'.'>,pt::one<'/'>>> {}; 
struct alphanum_no_space : pt::plus<pt::sor<pt::alnum,pt::one<'_'>,pt::one<'-'>,pt::one<'.'>,pt::one<'/'>>> {}; 
struct name :
    pt::sor< 
        pt::seq<
            pt::one<'"'> , alphanum_space , pt::one<'"'>
            >,
            alphanum_no_space
            > {}; 
struct numbers : pt::plus<pt::digit> {};
struct end_line : pt::seq<pt::opt<start_value>,pt::pad<end_declaration,pt::opt<comment>>>{};

//::::::::::::::PARSER GENERAL:::::::::::::://
//::::::::::::::::BASIC RULE:::::::::::::::://
//:::::::::::::::::::::::::::::::::::::::::://

template<typename Rule>
using tok = pt::pad<Rule, pt::space>;

template<typename Nr1,typename Nr2>
using array = pt::seq<
    TAO_PEGTL_STRING("Array"),
    pt::one<'['>,
    Nr1,
    TAO_PEGTL_STRING(".."), 
    Nr2,
    pt::one<']'>,
    pt::seq<
        pt::space,
        TAO_PEGTL_STRING("of"),
        pt::space>
        >;

template<typename Rule>
using okt = pt::pad<pt::space, Rule>;


//::::::::::::::FINAL STRUCTURE::::::::::::://
//:::::::::::::::::::::::::::::::::::::::::://
//:::::::::::::::::::::::::::::::::::::::::://
using utf8_bom = pt::utf8::bom;

//Forward declaration
struct udt_header_declaration;
struct udt_raw_header;
struct udt_raw_basic;
struct udt_raw_struct;
struct db_header;
struct db_body;
struct db_footer;

struct complete_datablock : pt::seq<
    pt::opt<utf8_bom>,
    pt::plus<udt_raw_header>,
    db_header,
    pt::plus<db_body>,
    db_footer
    >{};



//:::::::::::::::UDTs DATABASE:::::::::::::://
//:::::::::::::::FINAL ELEMENT:::::::::::://
//:::::::::::::::::::::::::::::::::::::::::://

struct udt_header_declaration : TAO_PEGTL_STRING("TYPE"){};
struct udt_header_quoted_name : name {};
struct udt_header_end_declaration : TAO_PEGTL_STRING("END_TYPE"){};
struct udt_header_struct : TAO_PEGTL_STRING("STRUCT"){};

struct udt_raw_header : pt::seq<
    udt_header_declaration,
    pt::space,
    udt_header_quoted_name,pt::eol,
    version,
    indent,udt_header_struct,pt::eol,
    pt::plus<udt_raw_basic>,
    pt::eol,
    udt_header_end_declaration,pt::eol,pt::eol
    >{};

//:::::::::::::::UDTs DATABASE:::::::::::::://
//:::::::::::::::BASIC ELEMENT:::::::::::://
//:::::::::::::::::::::::::::::::::::::::::://

struct udt_raw_basic_name : name{};
struct udt_raw_basic_type : name{};
struct udt_raw_basic_struct_end:pt::seq<TAO_PEGTL_STRING("END_STRUCT;"),pt::eol>{};
struct udt_raw_basic_array_start_arr: numbers{};
struct udt_raw_basic_array_end_arr: numbers{};

struct udt_raw_basic : 
    pt::seq<
        pt::opt<indent>,
        pt::sor<
            udt_raw_basic_struct_end,
            pt::seq<
                udt_raw_basic_name,
                pt::opt<attribute>,
                separator,
                pt::opt<array<udt_raw_basic_array_start_arr,udt_raw_basic_array_end_arr>>,
                udt_raw_basic_type,
                
                pt::sor<
                    pt::seq<
                        start_value,
                        pt::sor<
                            pt::eol,
                            comment
                        >
                    >,
                    pt::seq<
                        end_declaration,
                        pt::sor<
                            pt::eol,
                            comment
                        >                        
                    >,
                    comment,
                    pt::eol
                >
            >
        >
    >{};


//::::::::::DATABLOCK DECLARATION::::::::::://    
//:::::::::::::::BASIC ELEMENT:::::::::::://
//:::::::::::::::::::::::::::::::::::::::::://

struct db_std_name : name{};
struct db_std_type : alphanum{};
struct db_std_end_line : end_line{};

struct db_std : pt::seq<
    db_std_name,
    pt::opt<attribute>,
    separator,
    db_std_type,
    db_std_end_line
    >{};    

//::::::::::DATABLOCK DECLARATION::::::::::://    
//:::::::::::::::HEADER ELEMENT:::::::::::://
//:::::::::::::::::::::::::::::::::::::::::://

struct db_header_declaration : TAO_PEGTL_STRING("DATA_BLOCK") {};
struct db_header_retain : TAO_PEGTL_STRING("NON_RETAIN") {};
struct db_header_name : name{};

struct db_header : pt::seq<
    db_header_declaration,
    space,
    db_header_name,
    pt::eol,
    pt::opt<attribute>,
    pt::eol,
    version,
    pt::opt<db_header_retain,pt::eol>,
    pt::opt<
        pt::seq<
            indent,
            TAO_PEGTL_STRING("STRUCT"),
            pt::opt<space>,
            pt::eol
        >
    >
>{};

//::::::::::DATABLOCK DECLARATION::::::::::://    
//:::::::::::::::BODY ELEMENT:::::::::::://
//:::::::::::::::::::::::::::::::::::::::::://

struct db_body_name : name{};
struct db_body_type : name{};
struct db_body_array_start : numbers{};
struct db_body_array_end : numbers{};
struct db_from_udt : name{};
struct db_body : pt::seq<
    pt::sor<
        pt::seq<
            indent,
            db_body_name,
            pt::opt<
                attribute
            >,
            separator,
            pt::opt<array<db_body_array_start,db_body_array_end>>,
            db_body_type,
            end_declaration,
            pt::opt<comment>
        >,
        db_from_udt
    >
>{};

//::::::::::DATABLOCK DECLARATION::::::::::://    
//:::::::::::::::FOOTER ELEMENT:::::::::::://
//:::::::::::::::::::::::::::::::::::::::::://
struct db_footer : pt::seq<
    pt::sor<
        pt::seq<pt::eol,TAO_PEGTL_STRING("BEGIN")>,
        pt::seq<
            indent,
            pt::if_must<
            TAO_PEGTL_STRING("END_STRUCT;")
            >,
            pt::until<pt::eof>
        >
    > 
>{};
