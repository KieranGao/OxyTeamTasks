#include "MailerGrpcClient.h"
#include "Logger.h"

MailerGrpcClient::MailerGrpcClient() {
    ConfigManager& config = ConfigManager::getInstance();
    std::string host = config["VerifyServer"]["host"];
    std::string port = config["VerifyServer"]["port"];
    rpc_pool_ = std::make_unique<MailerConnectPool>(5, host, port);
}

SendMailRsp MailerGrpcClient::sendMail(const std::string& email, const std::string& code) {
    SendMailReq request;
    SendMailRsp response;
    ClientContext context;
    request.set_email(email);
    request.set_code(code);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->SendMail(&context, request, &response);
    if (!status.ok()) {
        LOG_ERROR("Mailer RPC failed: {}", status.error_message());
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}
