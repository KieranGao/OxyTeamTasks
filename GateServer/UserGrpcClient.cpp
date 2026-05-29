#include "UserGrpcClient.h"
#include "Logger.h"

UserGrpcClient::UserGrpcClient() {
    auto& g_config = ConfigManager::getInstance();
    std::string host = g_config["UMSServer"]["host"];
    std::string port = g_config["UMSServer"]["port"];
    rpc_pool_ = std::make_unique<UserConnectPool>(8, host, port);
}

VerifyRsp UserGrpcClient::getVerifyCode(const std::string& email) {
    VerifyReq request;
    VerifyRsp response;
    ClientContext context;
    request.set_email(email);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->GetVerifyCode(&context, request, &response);
    if(!status.ok()) {
        LOG_ERROR("UserService GetVerifyCode RPC failed: {}", status.error_message());
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

RegisterRsp UserGrpcClient::registerUser(const std::string& username, const std::string& email, const std::string& password, const std::string& code) {
    RegisterReq request;
    RegisterRsp response;
    ClientContext context;
    request.set_username(username);
    request.set_email(email);
    request.set_password(password);
    request.set_code(code);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->Register(&context, request, &response);
    if(!status.ok()) {
        LOG_ERROR("UserService Register RPC failed: {}", status.error_message());
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

LoginRsp UserGrpcClient::login(const std::string& email, const std::string& password) {
    LoginReq request;
    LoginRsp response;
    ClientContext context;
    request.set_email(email);
    request.set_password(password);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->Login(&context, request, &response);
    if(!status.ok()) {
        LOG_ERROR("UserService Login RPC failed: {}", status.error_message());
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

ResetPassRsp UserGrpcClient::resetPass(const std::string& username, const std::string& email, const std::string& newPassword, const std::string& code) {
    ResetPassReq request;
    ResetPassRsp response;
    ClientContext context;
    request.set_username(username);
    request.set_email(email);
    request.set_new_password(newPassword);
    request.set_code(code);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->ResetPass(&context, request, &response);
    if(!status.ok()) {
        LOG_ERROR("UserService ResetPass RPC failed: {}", status.error_message());
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

ListPendingUsersRsp UserGrpcClient::listPendingUsers() {
    ListPendingUsersReq request;
    ListPendingUsersRsp response;
    ClientContext context;
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->ListPendingUsers(&context, request, &response);
    if(!status.ok()) {
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

ApproveUserRsp UserGrpcClient::approveUser(int uid, int role, int belong_team_id) {
    ApproveUserReq request;
    ApproveUserRsp response;
    ClientContext context;
    request.set_uid(uid);
    request.set_role(role);
    request.set_belong_team_id(belong_team_id);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->ApproveUser(&context, request, &response);
    if(!status.ok()) {
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

RejectUserRsp UserGrpcClient::rejectUser(int uid) {
    RejectUserReq request;
    RejectUserRsp response;
    ClientContext context;
    request.set_uid(uid);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->RejectUser(&context, request, &response);
    if(!status.ok()) {
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

SetUserRoleRsp UserGrpcClient::setUserRole(int uid, int role, int belong_team_id) {
    SetUserRoleReq request;
    SetUserRoleRsp response;
    ClientContext context;
    request.set_uid(uid);
    request.set_role(role);
    request.set_belong_team_id(belong_team_id);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->SetUserRole(&context, request, &response);
    if(!status.ok()) {
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

ListAllUsersRsp UserGrpcClient::listAllUsers() {
    ListAllUsersReq request;
    ListAllUsersRsp response;
    ClientContext context;
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->ListAllUsers(&context, request, &response);
    if(!status.ok()) {
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

UpdateTeamInfoRsp UserGrpcClient::updateTeamInfo(int uid, int belong_team_id) {
    UpdateTeamInfoReq request;
    UpdateTeamInfoRsp response;
    ClientContext context;
    request.set_uid(uid);
    request.set_belong_team_id(belong_team_id);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->UpdateTeamInfo(&context, request, &response);
    if(!status.ok()) {
        LOG_ERROR("UserService UpdateTeamInfo RPC failed: {}", status.error_message());
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}
