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

using message::UserService;
using message::StatusService;
using message::TaskService;

// Pool for UserService clients (GateServer → UMSServer)
class UserConnectPool {
public:
    UserConnectPool(size_t pool_size, std::string host, std::string port);
    ~UserConnectPool();
    std::unique_ptr<UserService::Stub> getStub();
    void returnStub(std::unique_ptr<UserService::Stub> stub);
    void stop();
private:
    std::queue<std::unique_ptr<UserService::Stub>> stubs_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> is_running_;
    std::string host_;
    std::string port_;
    size_t pool_size_;
};

// Pool for StatusService clients (GateServer → StatusServer)
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

// Pool for TaskService clients (GateServer → TaskServer)
class TaskConnectPool {
public:
    TaskConnectPool(size_t pool_size, std::string host, std::string port);
    ~TaskConnectPool();
    std::unique_ptr<TaskService::Stub> getStub();
    void returnStub(std::unique_ptr<TaskService::Stub> stub);
    void stop();
private:
    std::queue<std::unique_ptr<TaskService::Stub>> stubs_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> is_running_;
    std::string host_;
    std::string port_;
    size_t pool_size_;
};

#endif /* RPCCONNECTPOOL_H */
