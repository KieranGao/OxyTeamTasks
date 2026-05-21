#ifndef STATUSSERVICEIMPL
#define STATUSSERVICEIMPL

#include "grpcpp/grpcpp.h"
#include "message.grpc.pb.h"
#include "SegmentTree.h"
#include <set>
#include <mutex>
#include <queue>
#include "Global.h"

using grpc::Server;
using grpc::Status;
using grpc::ServerContext;
using grpc::ServerBuilder;

using message::AllocateReq;
using message::AllocateRsp;
using message::LoginReportReq;
using message::LoginReportRsp;
using message::StatusService;

class PushServer {
public:
    PushServer() : host(""), port(""), name(""), id(0) {}
    PushServer(const PushServer& cs) : host(cs.host), port(cs.port), name(cs.name), id(cs.id) {}
    PushServer& operator=(const PushServer& cs) {
        if(&cs == this) return *this;
        host = cs.host;
        name = cs.name;
        port = cs.port;
        id = cs.id;
        return *this;
    }
    std::string host;
    std::string port;
    std::string name;
    int id;
};

class StatusServiceImpl final : public StatusService::Service {
public:
    StatusServiceImpl();
    Status AllocatePushServer(ServerContext* context, const AllocateReq* req, AllocateRsp* resp) override;
    Status ReportLogin(ServerContext* context, const LoginReportReq* req, LoginReportRsp* resp) override;
private:
    void insertToken(int uid, std::string token);
    PushServer& getPushServer();
    void returnServer(PushServer& cs);
    std::unique_ptr<SegmentTree> SegTree_;
    std::unordered_map<std::string, PushServer> servers_;
    std::unordered_map<int, PushServer> servers_idx_;
    std::mutex server_mtx_;
    int server_cnt_;
};

#endif /* STATUSSERVICEIMPL */
