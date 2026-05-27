#ifndef STATUSSERVICEIMPL
#define STATUSSERVICEIMPL

#include "grpcpp/grpcpp.h"
#include "message.grpc.pb.h"
#include "SegmentTree.h"
#include <set>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <atomic>
#include "Global.h"

using grpc::Server;
using grpc::Status;
using grpc::ServerContext;
using grpc::ServerBuilder;

using message::AllocateReq;
using message::AllocateRsp;
using message::LoginReportReq;
using message::LoginReportRsp;
using message::ReportLogReq;
using message::ReportLogRsp;
using message::QueryLogsReq;
using message::QueryLogsRsp;
using message::HeartbeatReq;
using message::HeartbeatRsp;
using message::QueryServerStatusReq;
using message::QueryServerStatusRsp;
using message::DisconnectReq;
using message::DisconnectRsp;
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
    Status ReportLog(ServerContext* context, const ReportLogReq* req, ReportLogRsp* resp) override;
    Status QueryLogs(ServerContext* context, const QueryLogsReq* req, QueryLogsRsp* resp) override;
    Status ServerHeartbeat(ServerContext* context, const HeartbeatReq* req, HeartbeatRsp* resp) override;
    Status QueryServerStatus(ServerContext* context, const QueryServerStatusReq* req, QueryServerStatusRsp* resp) override;
    Status ReportDisconnect(ServerContext* context, const DisconnectReq* req, DisconnectRsp* resp) override;
private:
    bool insertToken(int uid, std::string token);
    PushServer& getPushServer();
    void returnServer(PushServer& cs);

    // 负载均衡
    std::unique_ptr<SegmentTree> SegTree_;
    std::vector<int> server_conns_;
    std::unordered_map<std::string, PushServer> servers_;
    std::unordered_map<int, PushServer> servers_idx_;
    std::mutex server_mtx_;
    int server_cnt_;
    std::string allocate_method_;

    // 服务器心跳状态
    struct ServerHeartbeatInfo {
        std::string service;
        std::string host;
        std::string port;
        int64_t last_heartbeat = 0;
        std::string status = "offline";
    };
    std::unordered_map<std::string, ServerHeartbeatInfo> server_status_;
    std::mutex server_status_mtx_;
    // 超过三十秒无响应，置为OFFLINE
    int heartbeat_timeout_secs_ = 30;
};

#endif /* STATUSSERVICEIMPL */
