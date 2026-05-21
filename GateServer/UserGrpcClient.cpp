#include "UserGrpcClient.h"

UserGrpcClient::UserGrpcClient() {
    auto& g_config = ConfigManager::getInstance();
    std::string host = g_config["UMSServer"]["host"];
    std::string port = g_config["UMSServer"]["port"];
    rpc_pool_ = std::make_unique<UserConnectPool>(5, host, port);
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
        std::cerr << "UserService GetVerifyCode RPC failed: " << status.error_message() << std::endl;
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
        std::cerr << "UserService Register RPC failed: " << status.error_message() << std::endl;
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
        std::cerr << "UserService Login RPC failed: " << status.error_message() << std::endl;
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
        std::cerr << "UserService ResetPass RPC failed: " << status.error_message() << std::endl;
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}
