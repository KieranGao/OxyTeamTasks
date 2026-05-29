#ifndef USERGRPCCLIENT_H
#define USERGRPCCLIENT_H

#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "Global.h"
#include "Singleton.h"
#include "RPCConnectPool.h"
#include "ConfigManager.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using message::RegisterReq;
using message::RegisterRsp;
using message::LoginReq;
using message::LoginRsp;
using message::ResetPassReq;
using message::ResetPassRsp;
using message::VerifyReq;
using message::VerifyRsp;
using message::ListPendingUsersReq;
using message::ListPendingUsersRsp;
using message::ApproveUserReq;
using message::ApproveUserRsp;
using message::RejectUserReq;
using message::RejectUserRsp;
using message::SetUserRoleReq;
using message::SetUserRoleRsp;
using message::ListAllUsersReq;
using message::ListAllUsersRsp;
using message::UpdateTeamInfoReq;
using message::UpdateTeamInfoRsp;
using message::UserService;

class UserGrpcClient : public Singleton<UserGrpcClient> {
    friend class Singleton<UserGrpcClient>;
public:
    VerifyRsp getVerifyCode(const std::string& email);
    RegisterRsp registerUser(const std::string& username, const std::string& email, const std::string& password, const std::string& code);
    LoginRsp login(const std::string& email, const std::string& password);
    ResetPassRsp resetPass(const std::string& username, const std::string& email, const std::string& newPassword, const std::string& code);
    ListPendingUsersRsp listPendingUsers();
    ApproveUserRsp approveUser(int uid, int role, int belong_team_id);
    RejectUserRsp rejectUser(int uid);
    SetUserRoleRsp setUserRole(int uid, int role, int belong_team_id);
    ListAllUsersRsp listAllUsers();
    UpdateTeamInfoRsp updateTeamInfo(int uid, int belong_team_id);
private:
    std::unique_ptr<UserConnectPool> rpc_pool_;
    UserGrpcClient();
};

#endif /* USERGRPCCLIENT_H */
