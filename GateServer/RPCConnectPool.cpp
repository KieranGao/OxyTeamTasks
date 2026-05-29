#include "RPCConnectPool.h"
#include "Logger.h"

UserConnectPool::UserConnectPool(size_t pool_size, std::string host, std::string port) : is_running_(true), host_(std::move(host)), port_(std::move(port)), pool_size_(pool_size)
{
    for(int i=0;i<pool_size;i++) {
        std::shared_ptr<Channel> channel = grpc::CreateChannel(host_ + ":" + port_, grpc::InsecureChannelCredentials());
        stubs_.emplace(UserService::NewStub(channel));
    }
}

UserConnectPool::~UserConnectPool() {
    stop();
    LOG_DEBUG("UserConnectPool destroyed");
}

std::unique_ptr<UserService::Stub> UserConnectPool::getStub() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cond_.wait_for(lock, std::chrono::seconds(5), [this](){ return !stubs_.empty() or !is_running_; })) {
        return nullptr;
    }
    if(!is_running_) return nullptr;
    auto stub = std::move(stubs_.front());
    stubs_.pop();
    return stub;
}

void UserConnectPool::returnStub(std::unique_ptr<UserService::Stub> stub) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(is_running_) {
        stubs_.push(std::move(stub));
        cond_.notify_one();
    }
}

void UserConnectPool::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    is_running_ = false;
    cond_.notify_all();
    while(!stubs_.empty()) stubs_.pop();
}

// --- StatusConnectPool (GateServer → StatusServer) ---

StatusConnectPool::StatusConnectPool(size_t pool_size, std::string host, std::string port)
    : is_running_(true), host_(std::move(host)), port_(std::move(port)), pool_size_(pool_size)
{
    for(int i=0;i<pool_size;i++) {
        std::shared_ptr<Channel> channel = grpc::CreateChannel(host_ + ":" + port_, grpc::InsecureChannelCredentials());
        stubs_.emplace(StatusService::NewStub(channel));
    }
}

StatusConnectPool::~StatusConnectPool() {
    stop();
    LOG_INFO("StatusConnectPool destroyed");
}

std::unique_ptr<StatusService::Stub> StatusConnectPool::getStub() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cond_.wait_for(lock, std::chrono::seconds(5), [this](){ return !stubs_.empty() or !is_running_; })) {
        return nullptr;
    }
    if(!is_running_) return nullptr;
    auto stub = std::move(stubs_.front());
    stubs_.pop();
    return stub;
}

void StatusConnectPool::returnStub(std::unique_ptr<StatusService::Stub> stub) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(is_running_) {
        stubs_.push(std::move(stub));
        cond_.notify_one();
    }
}

void StatusConnectPool::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    is_running_ = false;
    cond_.notify_all();
    while(!stubs_.empty()) stubs_.pop();
}

// --- TaskConnectPool (GateServer → TaskServer) ---

TaskConnectPool::TaskConnectPool(size_t pool_size, std::string host, std::string port)
    : is_running_(true), host_(std::move(host)), port_(std::move(port)), pool_size_(pool_size)
{
    for(int i=0;i<pool_size;i++) {
        std::shared_ptr<Channel> channel = grpc::CreateChannel(host_ + ":" + port_, grpc::InsecureChannelCredentials());
        stubs_.emplace(TaskService::NewStub(channel));
    }
}

TaskConnectPool::~TaskConnectPool() {
    stop();
}

std::unique_ptr<TaskService::Stub> TaskConnectPool::getStub() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cond_.wait_for(lock, std::chrono::seconds(5), [this](){ return !stubs_.empty() or !is_running_; })) {
        return nullptr;
    }
    if(!is_running_) return nullptr;
    auto stub = std::move(stubs_.front());
    stubs_.pop();
    return stub;
}

void TaskConnectPool::returnStub(std::unique_ptr<TaskService::Stub> stub) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(is_running_) {
        stubs_.push(std::move(stub));
        cond_.notify_one();
    }
}

void TaskConnectPool::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    is_running_ = false;
    cond_.notify_all();
    while(!stubs_.empty()) stubs_.pop();
}
