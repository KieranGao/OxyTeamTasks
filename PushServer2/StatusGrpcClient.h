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

using message::LoginReportReq;
using message::LoginReportRsp;
using message::ReportLogReq;
using message::ReportLogRsp;
using message::HeartbeatReq;
using message::HeartbeatRsp;
using message::DisconnectReq;
using message::DisconnectRsp;
using message::StatusService;

class StatusGrpcClient : public Singleton<StatusGrpcClient> {
    friend class Singleton<StatusGrpcClient>;
public:
    LoginReportRsp reportLogin(int uid, const std::string& token, const std::string& server_name);
    void reportLog(const ReportLogReq& req);
    void heartbeat(const std::string& host, const std::string& port);
    void reportDisconnect(const std::string& server_name, int uid);
private:
    std::unique_ptr<StatusConnectPool> rpc_pool_;
    StatusGrpcClient();
};

#endif /* STATUSGRPCCLIENT_H */
