#include "StatusGrpcClient.h"

StatusGrpcClient::StatusGrpcClient() {
    auto& g_config = ConfigManager::getInstance();
    std::string host = g_config["StatusServer"]["host"];
    std::string port = g_config["StatusServer"]["port"];
    rpc_pool_ = std::make_unique<StatusConnectPool>(5, host, port);
}

AllocateRsp StatusGrpcClient::allocatePushServer(int uid) {
    AllocateReq request;
    AllocateRsp response;
    ClientContext context;
    request.set_uid(uid);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->AllocatePushServer(&context, request, &response);
    if(status.ok()) {
        return response;
    } else {
        response.set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
        return response;
    }
}
