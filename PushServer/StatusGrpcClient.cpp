#include "StatusGrpcClient.h"
#include "Logger.h"

StatusGrpcClient::StatusGrpcClient() {
    auto& g_config = ConfigManager::getInstance();
    std::string host = g_config["StatusServer"]["host"];
    std::string port = g_config["StatusServer"]["port"];
    rpc_pool_ = std::make_unique<StatusConnectPool>(8, host, port);
}

static void setDeadline(ClientContext& ctx, int seconds = 3) {
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(seconds);
    ctx.set_deadline(deadline);
}

LoginReportRsp StatusGrpcClient::reportLogin(int uid, const std::string& token, const std::string& server_name) {
    LoginReportReq request;
    LoginReportRsp reply;
    ClientContext context;
    setDeadline(context);
    request.set_uid(uid);
    request.set_token(token);
    request.set_server_name(server_name);
    auto stub = rpc_pool_->getStub();
    if (!stub) { reply.set_error(static_cast<int>(ErrorCodes::RPC_ERROR)); return reply; }
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->ReportLogin(&context, request, &reply);
    if (!status.ok()) {
        LOG_ERROR("[Push] ReportLogin gRPC failed: {}", status.error_message());
        reply.set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
    }
    return reply;
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
    request.set_service("PushServer");
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

void StatusGrpcClient::reportDisconnect(const std::string& server_name, int uid) {
    DisconnectReq request;
    DisconnectRsp response;
    ClientContext context;
    setDeadline(context);
    request.set_server_name(server_name);
    request.set_uid(uid);
    auto stub = rpc_pool_->getStub();
    if (!stub) {
        LOG_ERROR("[Push] reportDisconnect: no gRPC stub available for {}", server_name);
        return;
    }
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    grpc::Status status = stub->ReportDisconnect(&context, request, &response);
    if (!status.ok()) {
        LOG_ERROR("[Push] reportDisconnect gRPC failed for {}: {}", server_name, status.error_message());
    } else {
        LOG_INFO("[Push] reportDisconnect OK for {}", server_name);
    }
}
