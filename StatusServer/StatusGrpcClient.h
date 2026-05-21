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
using message::StatusService;

class StatusGrpcClient : public Singleton<StatusGrpcClient> {
    friend class Singleton<StatusGrpcClient>;
public:
    AllocateRsp allocatePushServer(int uid);
private:
    std::unique_ptr<StatusConnectPool> rpc_pool_;
    StatusGrpcClient();
};

#endif /* STATUSGRPCCLIENT_H */
