#pragma once

#include <opc_models.hpp>

inline bool asBool(const UA_Variant& v, bool& out){
    if(v.type == &UA_TYPES[UA_TYPES_BOOLEAN] && v.data){ out = *static_cast<UA_Boolean*>(v.data); return true; }
    return false;
}
inline bool asInt32(const UA_Variant& v, int32_t& out){
    if(v.type == &UA_TYPES[UA_TYPES_INT32] && v.data){ out = *static_cast<UA_Int32*>(v.data); return true; }
    return false;
}
inline bool asDouble(const UA_Variant& v, double& out){
    if(v.type == &UA_TYPES[UA_TYPES_DOUBLE] && v.data){ out = *static_cast<UA_Double*>(v.data); return true; }
    return false;
}