#ifndef PUSHGRPCSERVICEIMPL_H
#define PUSHGRPCSERVICEIMPL_H

#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "Global.h"

using grpc::Server;
using grpc::Status;
using grpc::ServerContext;
using grpc::ServerBuilder;

using message::PushToUserReq;
using message::PushToUserRsp;
using message::PushToTeamReq;
using message::PushToTeamRsp;
using message::GetMessagesReq;
using message::GetMessagesRsp;
using message::MarkReadReq;
using message::MarkReadRsp;
using message::DeleteMessageReq;
using message::DeleteMessageRsp;
using message::PushService;

class MainServer;  // forward declaration

class PushGrpcServiceImpl final : public PushService::Service {
public:
    Status PushToUser(ServerContext* context, const PushToUserReq* req, PushToUserRsp* resp) override;
    Status PushToTeam(ServerContext* context, const PushToTeamReq* req, PushToTeamRsp* resp) override;
    Status GetMessages(ServerContext* context, const GetMessagesReq* req, GetMessagesRsp* resp) override;
    Status MarkRead(ServerContext* context, const MarkReadReq* req, MarkReadRsp* resp) override;
    Status DeleteMessage(ServerContext* context, const DeleteMessageReq* req, DeleteMessageRsp* resp) override;
};

// Global setters called from PushServerMain.cpp
void setGlobalMainServer(MainServer* server);
void setGlobalGrpcPort(const std::string& port);

#endif /* PUSHGRPCSERVICEIMPL_H */
