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
using message::UserService;

class UMSGrpcServiceImpl final : public UserService::Service {
public:
    UMSGrpcServiceImpl();
    Status Register(ServerContext* context, const RegisterReq* req, RegisterRsp* resp) override;
    Status Login(ServerContext* context, const LoginReq* req, LoginRsp* resp) override;
    Status ResetPass(ServerContext* context, const ResetPassReq* req, ResetPassRsp* resp) override;
    Status GetVerifyCode(ServerContext* context, const VerifyReq* req, VerifyRsp* resp) override;
};

#endif /* UMSGRPCSERVICEIMPL_H */
