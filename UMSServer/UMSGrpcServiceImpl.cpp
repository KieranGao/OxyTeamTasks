#include "UMSGrpcServiceImpl.h"
#include "MailerGrpcClient.h"
#include "MySQLManager.h"
#include "RedisManager.h"
#include <string>
#include "Logger.h"

UMSGrpcServiceImpl::UMSGrpcServiceImpl() {}

// UMSSERVER SEND GVC REQ
Status UMSGrpcServiceImpl::GetVerifyCode(ServerContext* context, const VerifyReq* req, VerifyRsp* resp)
{
    std::string email = req->email();
    LOG_DEBUG("[UMS] GetVerifyCode for email: {}", email);
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
            LOG_ERROR("[UMS] Failed to store code in Redis");
            resp->set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
            resp->set_email(email);
            return Status::OK;
        }
    }
    // 通过MAILERSERVER发送验证码
    SendMailRsp mailResp = MailerGrpcClient::getInstance().sendMail(email, code);
    if (mailResp.error() != 0) {
        LOG_ERROR("[UMS] MailerServer returned error: {}", mailResp.error());
        resp->set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
        resp->set_email(email);
        return Status::OK;
    }

    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    resp->set_email(email);
    resp->set_code(code);
    LOG_DEBUG("[UMS] Verify code sent successfully");
    return Status::OK;
}


Status UMSGrpcServiceImpl::Register(ServerContext* context, const RegisterReq* req, RegisterRsp* resp)
{
    std::string username = req->username();
    std::string email = req->email();
    std::string password = req->password();
    std::string code = req->code();
    LOG_DEBUG("[UMS] Register user: {} email: {}", username, email);
 
    std::string storedCode;
    bool valid = RedisManager::getInstance().get(CODE_PREFIX + email, storedCode);
    if(!valid or storedCode != code) {
        LOG_WARN("[UMS] Verify code expired or mismatch");
        resp->set_error(static_cast<int>(ErrorCodes::VERIFY_CODE_EXPIRED));
        return Status::OK;
    }

    int uid = MySQLManager::getInstance().registerUser(username, email, password);
    if(uid <= 0) {
        LOG_ERROR("[UMS] Registration failed, uid={}", uid);
        resp->set_error(static_cast<int>(ErrorCodes::USER_ALREADY_EXISTS));
        return Status::OK;
    }
    // 成功注册后删除Redis中的验证码
    RedisManager::getInstance().del(CODE_PREFIX + email);

    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    LOG_INFO("[UMS] Registration success, uid={}", uid);
    return Status::OK;
}

// Login: verify credentials against MySQL
Status UMSGrpcServiceImpl::Login(ServerContext* context, const LoginReq* req, LoginRsp* resp)
{
    std::string email = req->email();
    std::string password = req->password();
    LOG_DEBUG("[UMS] Login attempt: {}", email);

    UserInfo userinfo;
    bool ok = MySQLManager::getInstance().checkLogin(email, password, userinfo);
    if (!ok) {
        LOG_ERROR("[UMS] Login failed: bad credentials");
        resp->set_error(static_cast<int>(ErrorCodes::USER_LOGIN_ERROR));
        return Status::OK;
    }

    if (userinfo.status != 1) {
        LOG_WARN("[UMS] Login rejected: user not approved (status={})", userinfo.status);
        resp->set_error(static_cast<int>(ErrorCodes::USER_NOT_APPROVED));
        resp->set_uid(userinfo.uid);
        resp->set_status(userinfo.status);
        return Status::OK;
    }

    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    resp->set_uid(userinfo.uid);
    resp->set_status(userinfo.status);
    LOG_DEBUG("[UMS] Login success, uid={}", userinfo.uid);
    return Status::OK;
}

// ResetPass: validate code → update password
Status UMSGrpcServiceImpl::ResetPass(ServerContext* context, const ResetPassReq* req, ResetPassRsp* resp)
{
    std::string username = req->username();
    std::string email = req->email();
    std::string newPassword = req->new_password();
    std::string code = req->code();
    LOG_DEBUG("[UMS] ResetPass for user: {} email: {}", username, email);

    std::string storedCode;
    bool valid = RedisManager::getInstance().get(CODE_PREFIX + email, storedCode);
    if(!valid or storedCode != code) {
        LOG_WARN("[UMS] Verify code expired or mismatch");
        resp->set_error(static_cast<int>(ErrorCodes::VERIFY_CODE_EXPIRED));
        return Status::OK;
    }
    bool ok = MySQLManager::getInstance().userResetpass(username, email, newPassword);
    if(!ok) {
        LOG_ERROR("[UMS] ResetPass failed: user/email mismatch");
        resp->set_error(static_cast<int>(ErrorCodes::USER_DO_NOT_EXISTS));
        return Status::OK;
    }
    RedisManager::getInstance().del(CODE_PREFIX + email);
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    LOG_DEBUG("[UMS] ResetPass success");
    return Status::OK;
}

Status UMSGrpcServiceImpl::ListPendingUsers(ServerContext* context, const ListPendingUsersReq* req, ListPendingUsersRsp* resp)
{
    LOG_DEBUG("[UMS] ListPendingUsers requested");
    std::vector<PendingUserInfo> users;
    bool ok = MySQLManager::getInstance().listPendingUsers(users);
    if (!ok) {
        resp->set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
        return Status::OK;
    }
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    for (auto& u : users) {
        auto* pu = resp->add_users();
        pu->set_uid(u.uid);
        pu->set_username(u.username);
        pu->set_email(u.email);
        pu->set_role(u.role);
        pu->set_belong_team_id(u.belong_team_id);
    }
    return Status::OK;
}

Status UMSGrpcServiceImpl::ApproveUser(ServerContext* context, const ApproveUserReq* req, ApproveUserRsp* resp)
{
    LOG_INFO("[UMS] ApproveUser uid={} role={} team={}", req->uid(), req->role(), req->belong_team_id());
    bool ok = MySQLManager::getInstance().approveUser(req->uid(), req->role(), req->belong_team_id());
    if (ok) {
        // 写穿：MySQL 更新成功后删除 Redis 缓存，下次登录重新加载
        RedisManager::getInstance().del("user_info:" + std::to_string(req->uid()));
    }
    resp->set_error(ok ? static_cast<int>(ErrorCodes::SUCCESS) : static_cast<int>(ErrorCodes::RPC_ERROR));
    return Status::OK;
}

Status UMSGrpcServiceImpl::RejectUser(ServerContext* context, const RejectUserReq* req, RejectUserRsp* resp)
{
    LOG_INFO("[UMS] RejectUser uid={}", req->uid());
    bool ok = MySQLManager::getInstance().rejectUser(req->uid());
    resp->set_error(ok ? static_cast<int>(ErrorCodes::SUCCESS) : static_cast<int>(ErrorCodes::RPC_ERROR));
    return Status::OK;
}

Status UMSGrpcServiceImpl::SetUserRole(ServerContext* context, const SetUserRoleReq* req, SetUserRoleRsp* resp)
{
    LOG_INFO("[UMS] SetUserRole uid={} role={} team={}", req->uid(), req->role(), req->belong_team_id());
    bool ok = MySQLManager::getInstance().setUserRole(req->uid(), req->role(), req->belong_team_id());
    if (ok) {
        RedisManager::getInstance().del("user_info:" + std::to_string(req->uid()));
    }
    resp->set_error(ok ? static_cast<int>(ErrorCodes::SUCCESS) : static_cast<int>(ErrorCodes::RPC_ERROR));
    return Status::OK;
}

Status UMSGrpcServiceImpl::ListAllUsers(ServerContext* context, const ListAllUsersReq* req, ListAllUsersRsp* resp)
{
    LOG_DEBUG("[UMS] ListAllUsers requested");
    std::vector<PendingUserInfo> users;
    bool ok = MySQLManager::getInstance().listAllUsers(users);
    if (!ok) {
        resp->set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
        return Status::OK;
    }
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    for (auto& u : users) {
        auto* pu = resp->add_users();
        pu->set_uid(u.uid);
        pu->set_username(u.username);
        pu->set_email(u.email);
        pu->set_role(u.role);
        pu->set_belong_team_id(u.belong_team_id);
        pu->set_status(u.status);
    }
    return Status::OK;
}

Status UMSGrpcServiceImpl::UpdateTeamInfo(ServerContext* context, const UpdateTeamInfoReq* req, UpdateTeamInfoRsp* resp)
{
    LOG_INFO("[UMS] UpdateTeamInfo uid={} team={}", req->uid(), req->belong_team_id());
    bool ok = MySQLManager::getInstance().updateUserTeam(req->uid(), req->belong_team_id());
    if (ok) {
        RedisManager::getInstance().del("user_info:" + std::to_string(req->uid()));
    }
    resp->set_error(ok ? static_cast<int>(ErrorCodes::SUCCESS) : static_cast<int>(ErrorCodes::USER_ID_INVALID));
    return Status::OK;
}
