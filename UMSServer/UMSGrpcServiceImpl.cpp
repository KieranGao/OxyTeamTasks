#include "UMSGrpcServiceImpl.h"
#include "MailerGrpcClient.h"
#include "MySQLManager.h"
#include "RedisManager.h"
#include <string>

UMSGrpcServiceImpl::UMSGrpcServiceImpl() {}

// GetVerifyCode: generate code → store in Redis → send email via MailerServer
Status UMSGrpcServiceImpl::GetVerifyCode(ServerContext* context, const VerifyReq* req, VerifyRsp* resp)
{
    std::string email = req->email();
    std::cerr << "[UMS] GetVerifyCode for email: " << email << std::endl;
    std::string code;
    bool exist = RedisManager::getInstance().get(CODE_PREFIX + email, code);
    if(!exist) {
        // 内存中没有就重新生成
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        code = std::to_string(ms % 10000);
        while(code.length() < 4) code = "0" + code;
        bool stored = RedisManager::getInstance().setex(CODE_PREFIX + email, code, 180);
        if(!stored) {
            std::cerr << "[UMS] Failed to store code in Redis" << std::endl;
            resp->set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
            resp->set_email(email);
            return Status::OK;
        }
    }

    // Send email via MailerServer
    SendMailRsp mailResp = MailerGrpcClient::getInstance().sendMail(email, code);
    if (mailResp.error() != 0) {
        std::cerr << "[UMS] MailerServer returned error: " << mailResp.error() << std::endl;
        resp->set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
        resp->set_email(email);
        return Status::OK;
    }

    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    resp->set_email(email);
    resp->set_code(code);
    std::cerr << "[UMS] Verify code sent successfully" << std::endl;
    return Status::OK;
}

// Register: validate code → call MySQL to register
Status UMSGrpcServiceImpl::Register(ServerContext* context, const RegisterReq* req, RegisterRsp* resp)
{
    std::string username = req->username();
    std::string email = req->email();
    std::string password = req->password();
    std::string code = req->code();
    std::cerr << "[UMS] Register user: " << username << " email: " << email << std::endl;
 
    std::string storedCode;
    bool valid = RedisManager::getInstance().get(CODE_PREFIX + email, storedCode);
    if(!valid or storedCode != code) {
        std::cerr << "[UMS] Verify code expired or mismatch" << std::endl;
        resp->set_error(static_cast<int>(ErrorCodes::VERIFY_CODE_EXPIRED));
        return Status::OK;
    }

    int uid = MySQLManager::getInstance().registerUser(username, email, password);
    if(uid <= 0) {
        std::cerr << "[UMS] Registration failed, uid=" << uid << std::endl;
        resp->set_error(static_cast<int>(ErrorCodes::USER_ALREADY_EXISTS));
        return Status::OK;
    }
    // 成功注册后删除Redis中的验证码
    RedisManager::getInstance().del(CODE_PREFIX + email);

    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    std::cerr << "[UMS] Registration success, uid=" << uid << std::endl;
    return Status::OK;
}

// Login: verify credentials against MySQL
Status UMSGrpcServiceImpl::Login(ServerContext* context, const LoginReq* req, LoginRsp* resp)
{
    std::string email = req->email();
    std::string password = req->password();
    std::cerr << "[UMS] Login attempt: " << email << std::endl;

    UserInfo userinfo;
    bool ok = MySQLManager::getInstance().checkLogin(email, password, userinfo);
    if (!ok) {
        std::cerr << "[UMS] Login failed: bad credentials" << std::endl;
        resp->set_error(static_cast<int>(ErrorCodes::USER_LOGIN_ERROR));
        return Status::OK;
    }

    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    resp->set_uid(userinfo.uid);
    std::cerr << "[UMS] Login success, uid=" << userinfo.uid << std::endl;
    return Status::OK;
}

// ResetPass: validate code → update password
Status UMSGrpcServiceImpl::ResetPass(ServerContext* context, const ResetPassReq* req, ResetPassRsp* resp)
{
    std::string username = req->username();
    std::string email = req->email();
    std::string newPassword = req->new_password();
    std::string code = req->code();
    std::cerr << "[UMS] ResetPass for user: " << username << " email: " << email << std::endl;

    std::string storedCode;
    bool valid = RedisManager::getInstance().get(CODE_PREFIX + email, storedCode);
    if(!valid or storedCode != code) {
        std::cerr << "[UMS] Verify code expired or mismatch" << std::endl;
        resp->set_error(static_cast<int>(ErrorCodes::VERIFY_CODE_EXPIRED));
        return Status::OK;
    }
    bool ok = MySQLManager::getInstance().userResetpass(username, email, newPassword);
    if(!ok) {
        std::cerr << "[UMS] ResetPass failed: user/email mismatch" << std::endl;
        resp->set_error(static_cast<int>(ErrorCodes::USER_DO_NOT_EXISTS));
        return Status::OK;
    }
    RedisManager::getInstance().del(CODE_PREFIX + email);
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    std::cerr << "[UMS] ResetPass success" << std::endl;
    return Status::OK;
}
