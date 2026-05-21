#ifndef MAILERGRPCCLIENT_H
#define MAILERGRPCCLIENT_H

#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "Global.h"
#include "Singleton.h"
#include "RPCConnectPool.h"
#include "ConfigManager.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using message::SendMailReq;
using message::SendMailRsp;
using message::MailerService;

class MailerGrpcClient : public Singleton<MailerGrpcClient> {
    friend class Singleton<MailerGrpcClient>;
public:
    SendMailRsp sendMail(const std::string& email, const std::string& code);
private:
    std::unique_ptr<MailerConnectPool> rpc_pool_;
    MailerGrpcClient();
};

#endif /* MAILERGRPCCLIENT_H */
