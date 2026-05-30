#ifndef RPCCONNECTPOOL_H
#define RPCCONNECTPOOL_H

#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <memory>
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

// 连接到其他 PushServer 节点的连接池（用于跨节点转发）
class PushConnectPool {
public:
    PushConnectPool(size_t pool_size, const std::string& host, const std::string& port);
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

// 管理到多个 PushServer 节点的连接（单例）
class PushNodeManager {
public:
    static PushNodeManager& getInstance();
    std::unique_ptr<PushService::Stub> getStub(const std::string& host, const std::string& grpc_port);
    void returnStub(const std::string& host, const std::string& grpc_port, std::unique_ptr<PushService::Stub> stub);
    void stop();
private:
    PushNodeManager() = default;
    std::unordered_map<std::string, std::shared_ptr<PushConnectPool>> pools_;
    std::mutex mtx_;
};

#endif /* RPCCONNECTPOOL_H */
