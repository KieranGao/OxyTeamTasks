#include "StatusGrpcClient.h"
#include "Logger.h"

StatusGrpcClient::StatusGrpcClient() {
    auto& g_config = ConfigManager::getInstance();
    std::string host = g_config["StatusServer"]["host"];
    std::string port = g_config["StatusServer"]["port"];
    rpc_pool_ = std::make_unique<StatusConnectPool>(8, host, port);
}

static void setDeadline(ClientContext& ctx, int seconds = 3) {
    ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(seconds));
}

AllocateRsp StatusGrpcClient::allocatePushServer(int uid) {
    AllocateReq request;
    AllocateRsp response;
    ClientContext context;
    setDeadline(context);
    request.set_uid(uid);
    auto stub = rpc_pool_->getStub();
    if (!stub) { response.set_error(static_cast<int>(ErrorCodes::RPC_ERROR)); return response; }
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->AllocatePushServer(&context, request, &response);
    if(status.ok()) {
        return response;
    } else {
        LOG_ERROR("[Gate] AllocatePushServer gRPC failed: {}", status.error_message());
        response.set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
        return response;
    }
}

LoginReportRsp StatusGrpcClient::reportLogin(int uid, std::string token) {
    LoginReportReq request;
    LoginReportRsp response;
    ClientContext context;
    setDeadline(context);
    request.set_uid(uid);
    request.set_token(token);
    auto stub = rpc_pool_->getStub();
    if (!stub) { response.set_error(static_cast<int>(ErrorCodes::RPC_ERROR)); return response; }
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->ReportLogin(&context, request, &response);
    if(status.ok()) {
        return response;
    } else {
        response.set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
        return response;
    }
}

QueryLogsRsp StatusGrpcClient::queryLogs(const std::string& service, const std::string& level, int limit) {
    QueryLogsReq request;
    QueryLogsRsp response;
    ClientContext context;
    setDeadline(context, 5);
    request.set_service(service);
    request.set_level(level);
    request.set_limit(limit);
    auto stub = rpc_pool_->getStub();
    if (!stub) { response.set_error(static_cast<int>(ErrorCodes::RPC_ERROR)); return response; }
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->QueryLogs(&context, request, &response);
    if (!status.ok()) {
        response.set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

QueryServerStatusRsp StatusGrpcClient::queryServerStatus() {
    QueryServerStatusReq request;
    QueryServerStatusRsp response;
    ClientContext context;
    setDeadline(context, 5);
    auto stub = rpc_pool_->getStub();
    if (!stub) { response.set_error(static_cast<int>(ErrorCodes::RPC_ERROR)); return response; }
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->QueryServerStatus(&context, request, &response);
    if (!status.ok()) {
        response.set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

void StatusGrpcClient::reportLog(const ReportLogReq& req) {
    ReportLogRsp response;
    ClientContext context;
    setDeadline(context, 5);
    auto stub = rpc_pool_->getStub();
    if (!stub) return;
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    stub->ReportLog(&context, req, &response);
}

void StatusGrpcClient::heartbeat(const std::string& host, const std::string& port) {
    HeartbeatReq request;
    HeartbeatRsp response;
    ClientContext context;
    setDeadline(context);
    request.set_service("GateServer");
    request.set_host(host);
    request.set_port(port);
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    request.set_timestamp(now);
    auto stub = rpc_pool_->getStub();
    if (!stub) return;
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->ServerHeartbeat(&context, request, &response);
    if (!status.ok()) {
        LOG_WARN("Heartbeat failed: {}", status.error_message());
    }
}
