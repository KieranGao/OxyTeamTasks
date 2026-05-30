#ifndef RPCCONNECTPOOL_H
#define RPCCONNECTPOOL_H

#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "Singleton.h"
#include "grpcpp/grpcpp.h"
#include "message.grpc.pb.h"
#include "Global.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using message::StatusService;
using message::PushService;

class StatusConnectPool {
public:
    StatusConnectPool(size_t pool_size, std::string host, std::string port);
    ~StatusConnectPool();
    std::unique_ptr<StatusService::Stub> getStub();
    void returnStub(std::unique_ptr<StatusService::Stub> stub);
    void stop();
private:
    std::queue<std::unique_ptr<StatusService::Stub>> stubs_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> is_running_;
    std::string host_;
    std::string port_;
    size_t pool_size_;
};

class PushConnectPool {
public:
    PushConnectPool(size_t pool_size, std::string host, std::string port);
    ~PushConnectPool();
    std::unique_ptr<PushService::Stub> getStub();
    void returnStub(std::unique_ptr<PushService::Stub> stub);
    void stop();
private:
    std::queue<std::unique_ptr<PushService::Stub>> stubs_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> is_running_;
    std::string host_;
    std::string port_;
    size_t pool_size_;
};

#endif /* RPCCONNECTPOOL_H */
