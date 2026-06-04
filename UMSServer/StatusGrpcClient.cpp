#include "StatusGrpcClient.h"
#include "Logger.h"

StatusGrpcClient::StatusGrpcClient() {
    auto& g_config = ConfigManager::getInstance();
    std::string host = g_config["StatusServer"]["host"];
    std::string port = g_config["StatusServer"]["port"];
    rpc_pool_ = std::make_unique<StatusConnectPool>(8, host, port);
}

ReportLogRsp StatusGrpcClient::reportLog(const ReportLogReq& req) {
    ReportLogRsp response;
    ClientContext context;
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->ReportLog(&context, req, &response);
    if (!status.ok()) {
        response.set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

void StatusGrpcClient::heartbeat(const std::string& host, const std::string& port) {
    HeartbeatReq request;
    HeartbeatRsp response;
    ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(3));
    request.set_service("UMSServer");
    request.set_host(host);
    request.set_port(port);
    request.set_timestamp(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    auto stub = rpc_pool_->getStub();
    if (!stub) return;
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->ServerHeartbeat(&context, request, &response);
    if (!status.ok()) {
        LOG_WARN("Heartbeat failed: {}", status.error_message());
    }
}
