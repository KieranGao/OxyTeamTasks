#ifndef VERIFYGRPCCLIENT_H
#define VERIFYGRPCCLIENT_H

#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h" 
#include "Global.h"
#include "Singleton.h"
#include "RPCConnectPool.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using message::GetVerifyReq;
using message::GetVerifyRsp;
using message::VerifyService;

class VerifyGrpcClient : public Singleton<VerifyGrpcClient> {
    friend class Singleton<VerifyGrpcClient>;
public:
    GetVerifyRsp getVerifyCode(const std::string& email);
private:
    std::unique_ptr<VerifyConnectPool> rpc_pool_;
    VerifyGrpcClient();
};

#endif /* VERIFYGRPCCLIENT_H */