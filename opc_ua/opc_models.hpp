#pragma once

#include <gui_utils.hpp>

extern "C" {
    #include <opc_ua/open62541.h>
}


struct ReadRequest {
    std::string nodeId;   
};

struct ReadResult {
    bool ok = false;
    std::string error;
    UA_Variant value{};   
};


class OpcUaDataSource {
public:
    using SubCallback = std::function<void(const std::string& nodeId, const UA_Variant& value)>;

    explicit OpcUaDataSource(std::string endpoint)
      : endpoint_(std::move(endpoint)) {}

    ~OpcUaDataSource() { disconnect(); }

    // opzionali: set credenziali / security prima del connect
    void setUserPass(std::string user, std::string pass) {
        user_ = std::move(user); pass_ = std::move(pass);
    }

    bool connect();
    void disconnect();
    bool isConnected() const { return client_ != nullptr; }

    ReadResult read(const ReadRequest& r) const;
    bool writeScalar(const std::string& nodeId, UA_UInt16 typeIndex, const void* data);

    // subscription: ritorna client handle; salva mapping nodeId->handle
    UA_UInt32 subscribe(const std::string& nodeId, SubCallback cb);
    void unsubscribe(UA_UInt32 clientHandle);

private:
    static void dataChangeHandler(UA_Client* client, UA_UInt32 subId, void* subCtx,
                                  UA_UInt32 monId, void* monCtx, UA_DataValue* dv);

    static bool parseNodeId(const std::string& s, UA_NodeId& out);
    static void variantClear(UA_Variant& v) { UA_Variant_clear(&v); }

private:
    UA_Client* client_ = nullptr;
    std::string endpoint_;
    std::optional<std::string> user_, pass_;

    struct MonInfo {
        std::string nodeId;
        SubCallback cb;
    };
    // handle → info
    std::unordered_map<UA_UInt32, MonInfo> subs_;
};