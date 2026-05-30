#include "PushGrpcClient.h"
#include "Logger.h"

PushGrpcClient::PushGrpcClient() {
    auto& g_config = ConfigManager::getInstance();
    std::string host = g_config["PushServer"]["host"];
    std::string port = g_config["PushServer"]["port"];
    rpc_pool_ = std::make_unique<PushConnectPool>(4, host, port);
}

PushToUserRsp PushGrpcClient::pushToUser(int uid, const std::string& msgType,
                                          const std::string& title, const std::string& payload) {
    PushToUserReq request;
    PushToUserRsp response;
    ClientContext context;
    request.set_uid(uid);
    request.set_msg_type(msgType);
    request.set_title(title);
    request.set_payload(payload);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status s = stub->PushToUser(&context, request, &response);
    if (!s.ok()) {
        LOG_ERROR("PushService PushToUser RPC failed: {}", s.error_message());
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

PushToTeamRsp PushGrpcClient::pushToTeam(int teamId, const std::string& msgType,
                                          const std::string& title, const std::string& payload,
                                          int excludeUid) {
    PushToTeamReq request;
    PushToTeamRsp response;
    ClientContext context;
    request.set_team_id(teamId);
    request.set_msg_type(msgType);
    request.set_title(title);
    request.set_payload(payload);
    request.set_exclude_uid(excludeUid);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status s = stub->PushToTeam(&context, request, &response);
    if (!s.ok()) {
        LOG_ERROR("PushService PushToTeam RPC failed: {}", s.error_message());
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}
