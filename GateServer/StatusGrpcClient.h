#ifndef STATUSGRPCCLIENT_H
#define STATUSGRPCCLIENT_H

#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "Global.h"
#include "Singleton.h"
#include "RPCConnectPool.h"
#include "ConfigManager.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using message::AllocateReq;
using message::AllocateRsp;
using message::LoginReportReq;
using message::LoginReportRsp;
using message::QueryLogsReq;
using message::QueryLogsRsp;
using message::QueryServerStatusReq;
using message::QueryServerStatusRsp;
using message::ReportLogReq;
using message::ReportLogRsp;
using message::HeartbeatReq;
using message::HeartbeatRsp;
using message::StatusService;

class StatusGrpcClient : public Singleton<StatusGrpcClient> {
    friend class Singleton<StatusGrpcClient>;
public:
    AllocateRsp allocatePushServer(int uid);
    LoginReportRsp reportLogin(int uid, std::string token);
    QueryLogsRsp queryLogs(const std::string& service, const std::string& level, int limit);
    QueryServerStatusRsp queryServerStatus();
    void reportLog(const ReportLogReq& req);
    void heartbeat(const std::string& host, const std::string& port);
private:
    std::unique_ptr<StatusConnectPool> rpc_pool_;
    StatusGrpcClient();
};

#endif /* STATUSGRPCCLIENT_H */
