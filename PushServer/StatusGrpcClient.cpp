#include "StatusGrpcClient.h"

StatusGrpcClient::StatusGrpcClient() {
    auto& g_config = ConfigManager::getInstance();
    std::string host = g_config["StatusServer"]["host"];
    std::string port = g_config["StatusServer"]["port"];
    rpc_pool_ = std::make_unique<StatusConnectPool>(5, host, port);
}

LoginReportRsp StatusGrpcClient::reportLogin(int uid, const std::string& token) {
    LoginReportReq request;
    LoginReportRsp reply;
    ClientContext context;
    request.set_uid(uid);
    request.set_token(token);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->ReportLogin(&context, request, &reply);
    if(status.ok()) {
        return reply;
    } else {
        reply.set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
        return reply;
    }
}
