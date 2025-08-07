from __future__ import annotations

from pyparsing import *
from data_type import tia_type_size

# Primitives
elememnt_name = Word(alphanums+'.-_/')|QuotedString('"')
element_type = Word(alphas+'"')
element_value= Suppress(Group(Literal(":")+Word(nums+".")))
value = Word(alphanums + '."[]')
element_attribute = Optional(Suppress("{" + SkipTo("}") + "}"))
spacer = Suppress(':')
std_element = oneOf(tia_type_size.keys())
udt_type = QuotedString('"')
number = Word(nums)
comment = Suppress("//"+ rest_of_line)
value_assignment = Suppress(":="+ SkipTo(";"))
end_of_line = Optional(value_assignment)+Suppress(";" + restOfLine)
element_header = elememnt_name("name")+element_attribute+spacer

arr_size = Group(Suppress("Array[")+number("start_array")+Suppress("..")+number("end_array")+Suppress("] of"))
# Primitives

#TYPE STANDARD
std_type= Group(elememnt_name("name")+element_attribute+spacer+std_element("type")+end_of_line+Optional(Suppress(Literal(":")+Word(nums+"."))))
#TYPE STANDARD

#TYPE UDT
udt_type= Group(elememnt_name("name")+element_attribute+spacer+QuotedString('"')("type")+end_of_line)
#TYPE UDT

#TYPE ARRAY 
array_type = Group(elememnt_name("name")+element_attribute+spacer+arr_size("array_length")+(QuotedString('"')|std_element)("type")+end_of_line)
#TYPE ARRAY

#TYPE STRUCT
struct_type_forward = Forward()
struct_type =  Group(elememnt_name("name") + element_attribute + spacer + Literal("Struct")("type") + Optional(comment)+Suppress(LineEnd()) +  OneOrMore(std_type | udt_type | array_type | struct_type_forward)("element") +Suppress("END_STRUCT;"))
struct_type_forward <<= struct_type
#TYPE STRUCT

#TYPE DB
db_header = Suppress("DATA_BLOCK")+Suppress(QuotedString('"'))+element_attribute+ Suppress("VERSION :")+Suppress(Word(nums+'.'))
db_struct= Suppress(db_header)+ Suppress("STRUCT")+ OneOrMore(std_type|array_type|udt_type)+Suppress("END_STRUCT;")
#TYPE DB

#TYPE UDT
udt_header = Suppress("TYPE")+QuotedString('"')("udt_name")+Optional(element_attribute)+ Suppress("VERSION")+Suppress(":")+Suppress(Word(nums+'.'))+Suppress("STRUCT")
udt_footer = Suppress("END_STRUCT;")+Empty()+Suppress("END_TYPE")
udt_forward = Forward()
udt_struct = udt_header+ OneOrMore(std_type|udt_type|array_type|struct_type|udt_forward)("udt")+ udt_footer
udt_forward <<= udt_struct
#TYPE UDT

# MAP DECLARATION
map_struct= OneOrMore(Group(udt_struct))("udt_field")+db_struct("struct_field")+Suppress("BEGIN"+SkipTo("END_DATA_BLOCK")+"END_DATA_BLOCK")+StringEnd()
# MAP DECLARATION






