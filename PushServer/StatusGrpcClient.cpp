#include "StatusGrpcClient.h"

StatusGrpcClient::StatusGrpcClient() {
    auto& g_config = ConfigManager::getInstance();
    std::string host = g_config["StatusServer"]["host"];
    std::string port = g_config["StatusServer"]["port"];
    // make_unique 是原子操作，内存分配和对象构造绑定在一起，不会产生内存泄漏
    // 如果用 rpc_pool_.reset(new VerifyConnectPool()), 若分配内存后构造函数抛异常，则会造成内存泄漏
    rpc_pool_ = std::make_unique<StatusConnectPool>(5, host, port);
}

GetPushServerRsp StatusGrpcClient::getPushServer(int uid) {
    GetPushServerReq request;
    GetPushServerRsp response;
    ClientContext context;
    request.set_uid(uid);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){
        rpc_pool_->returnStub(std::move(stub));
    });
    // 通过Stub使用gRPC通信，随后接收方将Response写入
    Status status = stub->GetPushServer(&context, request, &response);
    if(status.ok()) {
        return response;
    } else {
        response.set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
        return response;
    }
}

LoginRsp StatusGrpcClient::Login(int uid, const std::string& token)
{
	ClientContext context;
	LoginRsp reply;
	LoginReq request;
	request.set_uid(uid);
	request.set_token(token);
	auto stub = rpc_pool_->getStub();
	Status status = stub->Login(&context, request, &reply);
	Defer defer([&stub, this]() {
		rpc_pool_->returnStub(std::move(stub));
		});
	if (status.ok()) {
		return reply;
	}
	else {
		reply.set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
		return reply;
	}
}