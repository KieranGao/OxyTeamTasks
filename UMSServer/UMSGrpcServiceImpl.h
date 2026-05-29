#ifndef UMSGRPCSERVICEIMPL_H
#define UMSGRPCSERVICEIMPL_H

#include "grpcpp/grpcpp.h"
#include "message.grpc.pb.h"
#include "Global.h"

using grpc::Server;
using grpc::Status;
using grpc::ServerContext;
using grpc::ServerBuilder;

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

class UMSGrpcServiceImpl final : public UserService::Service {
public:
    UMSGrpcServiceImpl();
    Status Register(ServerContext* context, const RegisterReq* req, RegisterRsp* resp) override;
    Status Login(ServerContext* context, const LoginReq* req, LoginRsp* resp) override;
    Status ResetPass(ServerContext* context, const ResetPassReq* req, ResetPassRsp* resp) override;
    Status GetVerifyCode(ServerContext* context, const VerifyReq* req, VerifyRsp* resp) override;
    Status ListPendingUsers(ServerContext* context, const ListPendingUsersReq* req, ListPendingUsersRsp* resp) override;
    Status ApproveUser(ServerContext* context, const ApproveUserReq* req, ApproveUserRsp* resp) override;
    Status RejectUser(ServerContext* context, const RejectUserReq* req, RejectUserRsp* resp) override;
    Status SetUserRole(ServerContext* context, const SetUserRoleReq* req, SetUserRoleRsp* resp) override;
    Status ListAllUsers(ServerContext* context, const ListAllUsersReq* req, ListAllUsersRsp* resp) override;
    Status UpdateTeamInfo(ServerContext* context, const UpdateTeamInfoReq* req, UpdateTeamInfoRsp* resp) override;
};

#endif /* UMSGRPCSERVICEIMPL_H */
