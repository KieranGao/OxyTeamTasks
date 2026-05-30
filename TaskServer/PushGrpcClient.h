#ifndef PUSHGRPCCLIENT_H
#define PUSHGRPCCLIENT_H

#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "Global.h"
#include "Singleton.h"
#include "RPCConnectPool.h"
#include "ConfigManager.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using message::PushToUserReq;
using message::PushToUserRsp;
using message::PushToTeamReq;
using message::PushToTeamRsp;
using message::PushService;

class PushGrpcClient : public Singleton<PushGrpcClient> {
    friend class Singleton<PushGrpcClient>;
public:
    PushToUserRsp pushToUser(int uid, const std::string& msgType,
                             const std::string& title, const std::string& payload);
    PushToTeamRsp pushToTeam(int teamId, const std::string& msgType,
                             const std::string& title, const std::string& payload, int excludeUid);
private:
    std::unique_ptr<PushConnectPool> rpc_pool_;
    PushGrpcClient();
};

#endif /* PUSHGRPCCLIENT_H */
