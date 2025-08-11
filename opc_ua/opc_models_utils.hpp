#pragma once 

#include <opc_funcionality.hpp>


bool OpcUaDataSource::connect()
{
    disconnect();

    client_ = UA_Client_new();
    auto* cfg = UA_Client_getConfig(client_);
    UA_ClientConfig_setDefault(cfg);

    UA_StatusCode st;
    if (user_ && pass_) {
        // auth username/password, security None (per partire)
        st = UA_Client_connectUsername(client_, endpoint_.c_str(), user_->c_str(), pass_->c_str());
    } else {
        st = UA_Client_connect(client_, endpoint_.c_str());
    }

    if (st != UA_STATUSCODE_GOOD) {
        UA_Client_delete(client_); client_ = nullptr;
        return false;
    }
    return true;
}

void OpcUaDataSource::disconnect()
{
    if (client_) {
        UA_Client_disconnect(client_);
        UA_Client_delete(client_);
        client_ = nullptr;
        subs_.clear();
    }
}

bool parseNodeId(const std::string& s, UA_NodeId& out) {
    unsigned ns = 0;
    char type = 0;
    char value[256] = {0};

    if (sscanf(s.c_str(), "ns=%u;%c=%255[^\n]", &ns, &type, value) >= 3) {
        if (type == 's') {
            out = UA_NODEID_STRING_ALLOC((UA_UInt16)ns, value);
            return true;
        } else if (type == 'i') {
            unsigned num = std::stoul(value);
            out = UA_NODEID_NUMERIC((UA_UInt16)ns, num);
            return true;
        }
    }
    return false;
}

ReadResult OpcUaDataSource::read(const ReadRequest& r) const
{
    ReadResult res;
    if (!client_) { res.error = "Not connected"; return res; }

    UA_NodeId nid;
    if (!parseNodeId(r.nodeId, nid)) { res.error = "Bad NodeId"; return res; }

    UA_Variant_init(&res.value);
    UA_StatusCode st = UA_Client_readValueAttribute(client_, nid, &res.value);
    UA_NodeId_clear(&nid);

    if (st == UA_STATUSCODE_GOOD && res.value.data) {
        res.ok = true;
    } else {
        res.error = "Read failed";
    }
    return res;
}

// typeIndex: indice di UA_TYPES (es: UA_TYPES_INT32)
bool OpcUaDataSource::writeScalar(const std::string& nodeId, UA_UInt16 typeIndex, const void* data)
{
    if (!client_) return false;

    UA_NodeId nid;
    if (!parseNodeId(nodeId, nid)) return false;

    UA_Variant v; UA_Variant_init(&v);
    UA_Variant_setScalarCopy(&v, data, &UA_TYPES[typeIndex]);
    UA_StatusCode st = UA_Client_writeValueAttribute(client_, nid, &v);

    UA_Variant_clear(&v);
    UA_NodeId_clear(&nid);
    return st == UA_STATUSCODE_GOOD;
}

UA_UInt32 OpcUaDataSource::subscribe(const std::string& nodeId, SubCallback cb)
{
    if (!client_) return 0;

    // crea (o riusa) una subscription
    UA_CreateSubscriptionRequest req = UA_CreateSubscriptionRequest_default();
    UA_CreateSubscriptionResponse resp = UA_Client_Subscriptions_create(client_, req, nullptr, nullptr, nullptr);
    if (resp.responseHeader.serviceResult != UA_STATUSCODE_GOOD) return 0;

    // MonitoredItem
    UA_NodeId nid;
    if (!parseNodeId(nodeId, nid)) return 0;

    UA_MonitoredItemCreateRequest monReq = UA_MonitoredItemCreateRequest_default(nid);
    // client handle = a tua scelta; usiamo un numero casuale semplice
    UA_UInt32 clientHandle = (UA_UInt32)std::hash<std::string>{}(nodeId);

    UA_TimestampsToReturn ua_time ;
    monReq.requestedParameters.clientHandle = clientHandle;
    UA_MonitoredItemCreateResult monRes =
        UA_Client_MonitoredItems_createDataChange(client_, resp.subscriptionId,
                                                  ua_time,
                                                  monReq, this, dataChangeHandler, nullptr);

    UA_NodeId_clear(&nid);
    if (monRes.statusCode != UA_STATUSCODE_GOOD) return 0;

    subs_[clientHandle] = MonInfo{ nodeId, std::move(cb) };
    return clientHandle;
}

void OpcUaDataSource::unsubscribe(UA_UInt32 clientHandle)
{
    if (!client_) return;
    subs_.erase(clientHandle);
}

// callback static
void OpcUaDataSource::dataChangeHandler(UA_Client* client, UA_UInt32 subId, void* subCtx,
                                        UA_UInt32 monId, void* monCtx, UA_DataValue* dv)
{
    (void)subId; (void)monCtx;
    auto* self = static_cast<OpcUaDataSource*>(subCtx);
    if (!self) return;

    auto it = self->subs_.find(monId);
    if (it == self->subs_.end()) return;

    if (dv && UA_Variant_isScalar(&dv->value) && dv->value.data) {
        it->second.cb(it->second.nodeId, dv->value);
    }
}