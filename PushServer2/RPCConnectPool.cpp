#include "RPCConnectPool.h"
#include "Logger.h"

StatusConnectPool::StatusConnectPool(size_t pool_size, std::string host, std::string port) 
    : is_running_(true), host_(std::move(host)), port_(std::move(port)), pool_size_(pool_size) 
{
    for(int i=0;i<pool_size;i++) {
        std::shared_ptr<Channel> channel = grpc::CreateChannel(host_ + ":" + port_, grpc::InsecureChannelCredentials());
        stubs_.emplace(StatusService::NewStub(channel));
    }
}

StatusConnectPool::~StatusConnectPool() {
    // std::lock_guard<std::mutex> lock(mutex_); 不可加，会产生死锁
    stop();
    LOG_DEBUG("StatusConnectPool is destroyed");
}

std::unique_ptr<StatusService::Stub> StatusConnectPool::getStub() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cond_.wait_for(lock, std::chrono::seconds(5), [this](){ return !stubs_.empty() or !is_running_; })) {
        return nullptr;
    }
    if(!is_running_) {
        return nullptr; // 连接池已停止，返回空指针
    }
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
    while(!stubs_.empty()) {
        stubs_.pop();
    }
}

// ---- PushConnectPool ----

PushConnectPool::PushConnectPool(size_t pool_size, const std::string& host, const std::string& port)
    : is_running_(true), host_(host), port_(port), pool_size_(pool_size)
{
    for (size_t i = 0; i < pool_size; i++) {
        auto channel = grpc::CreateChannel(host_ + ":" + port_, grpc::InsecureChannelCredentials());
        stubs_.emplace(PushService::NewStub(channel));
    }
}

PushConnectPool::~PushConnectPool() {
    stop();
}

std::unique_ptr<PushService::Stub> PushConnectPool::getStub() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cond_.wait_for(lock, std::chrono::seconds(5), [this]() { return !stubs_.empty() || !is_running_; })) {
        return nullptr;
    }
    if (!is_running_) return nullptr;
    auto stub = std::move(stubs_.front());
    stubs_.pop();
    return stub;
}

void PushConnectPool::returnStub(std::unique_ptr<PushService::Stub> stub) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_running_) {
        stubs_.push(std::move(stub));
        cond_.notify_one();
    }
}

void PushConnectPool::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    is_running_ = false;
    cond_.notify_all();
    while (!stubs_.empty()) stubs_.pop();
}

// ---- PushNodeManager ----

PushNodeManager& PushNodeManager::getInstance() {
    static PushNodeManager instance;
    return instance;
}

std::unique_ptr<PushService::Stub> PushNodeManager::getStub(const std::string& host, const std::string& grpc_port) {
    std::string key = host + ":" + grpc_port;
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = pools_.find(key);
    if (it == pools_.end()) {
        auto pool = std::make_shared<PushConnectPool>(4, host, grpc_port);
        pools_[key] = pool;
        return pool->getStub();
    }
    return it->second->getStub();
}

void PushNodeManager::returnStub(const std::string& host, const std::string& grpc_port, std::unique_ptr<PushService::Stub> stub) {
    std::string key = host + ":" + grpc_port;
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = pools_.find(key);
    if (it != pools_.end()) {
        it->second->returnStub(std::move(stub));
    }
}

void PushNodeManager::stop() {
    std::lock_guard<std::mutex> lock(mtx_);
    for (auto& [key, pool] : pools_) {
        pool->stop();
    }
    pools_.clear();
}