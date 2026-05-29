#ifndef STATUSGRPCCLIENT_UMS_H
#define STATUSGRPCCLIENT_UMS_H

#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "Global.h"
#include "Singleton.h"
#include "RPCConnectPool.h"
#include "ConfigManager.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using message::ReportLogReq;
using message::ReportLogRsp;
using message::HeartbeatReq;
using message::HeartbeatRsp;
using message::StatusService;

class StatusGrpcClient : public Singleton<StatusGrpcClient> {
    friend class Singleton<StatusGrpcClient>;
public:
    ReportLogRsp reportLog(const ReportLogReq& req);
    void heartbeat(const std::string& host, const std::string& port);
private:
    std::unique_ptr<StatusConnectPool> rpc_pool_;
    StatusGrpcClient();
};

#endif

