/**
 * @file parser.hpp
 * @brief PEGTL-based grammar and actions to parse Siemens TIA Portal datablock sources (.db).
 *
 * @details
 * This parser uses PEGTL rules (seq/sor/opt/plus/etc.) to match the textual format of TIA
 * datablocks and UDTs, and builds a hierarchical in-memory model (DB/UDT/fields).
 * Actions (see parser actions .cpp) materialize matched tokens into C++ objects by
 * mutating a shared ParserState (scope stack + transient fields + UDT registry).
 *
 * -----------------------------------------------------------------------------
 * Core Concepts
 * -----------------------------------------------------------------------------
 * - Grammar rules:
 *   Each rule in this header matches a token/production (e.g., db_header, udt_raw_basic, ...).
 *   They define syntax only; object construction happens in specialized action<Rule> in the .cpp.
 *
 * - Actions:
 *   action<Rule>::apply(Input, ParserState) is invoked on successful matches.
 *   Actions read tokens and update ParserState (transient fields, open scopes, UDT map),
 *   or create/insert concrete nodes (STD_SINGLE/ARRAY, UDT_SINGLE/ARRAY, STRUCT_SINGLE/ARRAY).
 *
 * - ParserState (high level):
 *     - std::string name, type; bool is_arr; int array_start, array_end
 *       (transient fields captured by grammar until ';').
 *     - std::vector<ScopeVariant> element_in_scope
 *       (a stack of open containers: DB, UDT_RAW, STRUCT_*; top = current).
 *     - UdtRawMap udt_database (UDT registry by name).
 *     - std::shared_ptr<DB> db (the root DB being built).
 *
 * -----------------------------------------------------------------------------
 * Scope Handling
 * -----------------------------------------------------------------------------
 * - Opening constructs push on stack:
 *     TYPE <name>      -> push UDT_RAW
 *     DATA_BLOCK <...> -> push DB
 *     STRUCT           -> push STRUCT_SINGLE
 *
 * - Field emission (at ';'):
 *     ParserState::insert_element_inscope() inspects transient state:
 *       • Standard types -> STD_SINGLE or STD_ARRAY
 *       • UDT references -> UDT_SINGLE or UDT_ARRAY
 *     Then clears transient state (clear_state()).
 *
 * - Closing constructs pop from stack:
 *     END_STRUCT; -> pop top. If parent is UDT_RAW/DB, insert popped STRUCT as child.
 *     If a top-level UDT_RAW is closed, it is registered into udt_database.
 *
 * -----------------------------------------------------------------------------
 * Example Flow
 * -----------------------------------------------------------------------------
 * @code
 * DATA_BLOCK "MyDB"
 * VERSION : 1.0
 *   Var1 : Int;
 *   Var2 : Bool;
 * END_STRUCT;
 * @endcode
 * Steps:
 *   1) db_header_declaration -> create DB("MyDB"), push onto scope stack
 *   2) db_body_name/type + end_declaration -> insert STD_SINGLE("Var1","Int")
 *   3) repeat for "Var2"
 *   4) END_STRUCT -> pop STRUCT, DB is finalized
 *
 * -----------------------------------------------------------------------------
 * Minimal Structure (ASCII)
 * -----------------------------------------------------------------------------
 *   DB("MyDB")
 *   ├── STD_SINGLE("Var1":"Int")
 *   └── STD_SINGLE("Var2":"Bool")
 *
 * UDT example (conceptual):
 *   UDT_RAW("MyUDT")
 *   └── STRUCT_SINGLE("MyUDT","Struct")
 *       ├── STD_SINGLE("FieldA":"Int")
 *       └── STD_ARRAY ("ArrayB[0..3]":"Bool")
 *
 * -----------------------------------------------------------------------------
 * Key Takeaways
 * -----------------------------------------------------------------------------
 * - Grammar describes syntax; actions build the object model.
 * - The scope stack (element_in_scope) tracks the current insertion point.
 * - Transient fields (name/type/array) are cleared after each ';'.
 * - udt_database enables UDT references within DB bodies.
 *
 * @note Endianness & sizes are handled when decoding values via `translate::*` and
 *       the `tia_type_size` lookup (see classes.cpp). Adjust to your PLC data format.
 * @note If you change quoting/normalization of names in grammar, keep actions consistent
 *       (e.g., UDT detection vs. standard types).
 */
#pragma once

#include <tao/pegtl.hpp>
#include <tao/pegtl/utf8.hpp>
#include <tao/pegtl/contrib/parse_tree.hpp>
#include "datatype.hpp"

namespace pt = tao::pegtl;


// ===================== PARSER GENERAL / BASIC ELEMENT =====================

/// \brief Alias for PEGTL space rule.
/// \details Matches standard whitespace.
using space = pt::space ;

/// \brief One or more spaces (indentation).
struct indent : pt::plus<space>{};

/// \brief Separator ` : ` with optional spaces around colon.
struct separator : pt::seq<space,pt::one<':'>,space> {};

/// \brief End of declaration `;`.
struct end_declaration : pt::one<';'> {};

/// \brief Attribute block `{ ... }`.
/// \details Opens with `{` and consumes everything until matching `}`.
/// Optional leading spaces allowed.
struct attribute : pt::seq<pt::opt<space>, pt::if_must<pt::one<'{'>, pt::until<pt::one<'}'>>> >{};

/// \brief Line comment starting with `//` until end-of-line.
/// \details Requires three spaces before `//` (following code convention).
struct comment : pt::if_must<pt::three<' '>,TAO_PEGTL_STRING("//"),pt::until<pt::eol>>{};

/// \brief Value assignment `:= ... ;`.
/// \details Consumes `:=` followed by any content until a `;`.
struct start_value :pt::seq<space, pt::if_must<TAO_PEGTL_STRING(":="),pt::until<end_declaration>>> {};

/// \brief Version keyword with newline, e.g. `VERSION : 1.0`.
struct version : pt::seq<TAO_PEGTL_STRING("VERSION"),separator,pt::plus<pt::digit>,pt::one<'.'>,pt::plus<pt::digit>,pt::eol>{};

/// \brief Version keyword without newline (inline usage).
struct version_noeol : pt::seq<TAO_PEGTL_STRING("VERSION"),separator,pt::plus<pt::digit>,pt::one<'.'>,pt::plus<pt::digit>>{};

/// \brief Extended alphanumeric token (includes `_ - . /`).
struct alphanum : pt::plus<pt::sor<pt::alnum,pt::one<'_'>,pt::one<'-'>,pt::one<'.'>,pt::one<'/'>,pt::one<'&'>>> {}; 

/// \brief Like \ref alphanum but allows spaces (for quoted names).
struct alphanum_space : pt::plus<pt::sor<pt::alnum,pt::one<'_'>,space,pt::one<'-'>,pt::one<'.'>,pt::one<'/'>,pt::one<'&'>>> {}; 

/// \brief Like \ref alphanum but without spaces (for unquoted names).
struct alphanum_no_space : pt::plus<pt::sor<pt::alnum,pt::one<'_'>,pt::one<'-'>,pt::one<'.'>,pt::one<'/'>>> {}; 

/// \brief Name: either quoted with spaces allowed, or unquoted without spaces.
struct name :
    pt::sor< 
        pt::seq<
            pt::one<'"'> ,
            alphanum_space , 
            pt::one<'"'>
        >,
        alphanum_no_space
    > {}; 

/// \brief Sequence of digits.
struct numbers : pt::plus<pt::digit> {};

/// \brief End-of-line: optional `:=` assignment, mandatory `;`, optional comment.
struct end_line : pt::seq<pt::opt<start_value>,pt::pad<end_declaration,pt::opt<comment>>>{};


// ===================== BASIC RULE WRAPPERS / TEMPLATES =====================

/// \brief Wrapper that applies padding spaces around a rule.
template<typename Rule>
using tok = pt::pad<Rule, pt::space>;

/// \brief Array declaration `Array[<Nr1>.. <Nr2>] of`.
/// \tparam Nr1 Rule producing the start index.
/// \tparam Nr2 Rule producing the end index.
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
/// \brief Alternative padding wrapper (spaces before a rule).
template<typename Rule>
using okt = pt::pad<pt::space, Rule>;


// ===================== FINAL STRUCTURE (ROOT GRAMMAR) =====================

/// \brief Optional UTF-8 BOM at start of file.
using utf8_bom = pt::utf8::bom;

/// \brief Forward declarations of main grammar rules.
struct udt_header_declaration;
struct udt_raw_header;
struct udt_raw_basic;
struct udt_raw_struct;
struct db_header;
struct db_body;
struct db_footer;

/// \brief Root rule for a complete datablock file.
/// \details Structure: [BOM] + one or more UDTs + DB header + DB body + DB footer.
struct complete_datablock : pt::seq<
    pt::opt<utf8_bom>,
    pt::plus<udt_raw_header>,
    db_header,
    pt::plus<db_body>,
    db_footer
    >{};


// ===================== UDTs DATABASE =====================

/// \brief Keyword `TYPE` introducing a UDT.
struct udt_header_declaration : TAO_PEGTL_STRING("TYPE"){};

/// \brief Quoted or unquoted UDT name.
struct udt_header_quoted_name : name {};

/// \brief Keyword `END_TYPE` closing a UDT.
struct udt_header_end_declaration : TAO_PEGTL_STRING("END_TYPE"){};

/// \brief Keyword `STRUCT` starting a UDT body.
struct udt_header_struct : TAO_PEGTL_STRING("STRUCT"){};

/// \brief Full UDT header and body: `TYPE <name>` + `VERSION` + `STRUCT` + fields + `END_TYPE`.
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


    

/// \brief Field name inside UDT.
struct udt_raw_basic_name : name{};

/// \brief Field type inside UDT (base type or another UDT).
struct udt_raw_basic_type : name{};

/// \brief End of struct marker: `END_STRUCT;`.
struct udt_raw_basic_struct_end:pt::seq<TAO_PEGTL_STRING("END_STRUCT;"),pt::eol>{};

/// \brief Start index of an array field.
struct udt_raw_basic_array_start_arr: numbers{};

/// \brief End index of an array field.
struct udt_raw_basic_array_end_arr: numbers{};

/// \brief Field definition or struct terminator inside a UDT.
/// \details Handles attributes `{}`, optional `Array[..] of`, value assignment `:=`,  
/// or just `;` with optional comments or newlines. Also accepts `END_STRUCT;`.
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


// ===================== DATABASE DECLARATION =====================


/// \brief DB field name.
struct db_std_name : name{};

/// \brief DB field type (alphanumeric without spaces).
struct db_std_type : alphanum{};

/// \brief End of line for DB field, with optional assignment and comment.
struct db_std_end_line : end_line{};

/// \brief Standard DB field definition: `<name> [:attributes] : <type> ;`.
struct db_std : pt::seq<
    db_std_name,
    pt::opt<attribute>,
    separator,
    db_std_type,
    db_std_end_line
    >{};    

// ===================== DB HEADER =====================

/// \brief Keyword `DATA_BLOCK` starting a DB.
struct db_header_declaration : TAO_PEGTL_STRING("DATA_BLOCK") {};

/// \brief Optional `NON_RETAIN` flag.
struct db_header_retain : TAO_PEGTL_STRING("NON_RETAIN") {};

/// \brief DB name (quoted or unquoted).
struct db_header_name : name{};

/// \brief Full DB header: `DATA_BLOCK <name>` + attributes + `VERSION` + [NON_RETAIN] + optional `STRUCT`.
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

// ===================== DB BODY =====================

/// \brief DB body field name.
struct db_body_name : name{};

/// \brief DB body field type (base type or UDT).
struct db_body_type : name{};

/// \brief Start index of DB array field.
struct db_body_array_start : numbers{};

/// \brief End index of DB array field.
struct db_body_array_end : numbers{};

/// \brief Direct inclusion of a UDT inside DB.
struct db_from_udt : name{};

/// \brief DB body rule: structured field or UDT reference.
/// \details Supports attributes, `Array[..] of` ranges, `;`, and optional comment.
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

// ===================== DB FOOTER =====================

/// \brief DB footer: either `BEGIN` or `END_STRUCT;` until EOF.
/// \details Covers both DB export variants (with data or without).
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
