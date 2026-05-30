#include "RPCConnectPool.h"
#include "Logger.h"
#include <chrono>

StatusConnectPool::StatusConnectPool(size_t pool_size, std::string host, std::string port) : is_running_(true), host_(std::move(host)), port_(std::move(port)), pool_size_(pool_size)
{
    for (int i = 0; i < (int)pool_size; i++) {
        std::shared_ptr<Channel> channel = grpc::CreateChannel(host_ + ":" + port_, grpc::InsecureChannelCredentials());
        stubs_.emplace(StatusService::NewStub(channel));
    }
}

StatusConnectPool::~StatusConnectPool() { stop(); }

std::unique_ptr<StatusService::Stub> StatusConnectPool::getStub() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cond_.wait_for(lock, std::chrono::seconds(3), [this]() { return !stubs_.empty() || !is_running_; })) {
        return nullptr;
    }
    if (!is_running_) return nullptr;
    auto stub = std::move(stubs_.front());
    stubs_.pop();
    return stub;
}

void StatusConnectPool::returnStub(std::unique_ptr<StatusService::Stub> stub) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_running_) { stubs_.push(std::move(stub)); cond_.notify_one(); }
}

void StatusConnectPool::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    is_running_ = false;
    cond_.notify_all();
    while (!stubs_.empty()) stubs_.pop();
}

PushConnectPool::PushConnectPool(size_t pool_size, std::string host, std::string port)
    : is_running_(true), host_(std::move(host)), port_(std::move(port)), pool_size_(pool_size) {
    for (size_t i = 0; i < pool_size; i++) {
        std::shared_ptr<Channel> channel = grpc::CreateChannel(host_ + ":" + port_, grpc::InsecureChannelCredentials());
        stubs_.emplace(PushService::NewStub(channel));
    }
}

PushConnectPool::~PushConnectPool() { stop(); }

std::unique_ptr<PushService::Stub> PushConnectPool::getStub() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cond_.wait_for(lock, std::chrono::seconds(3), [this]() { return !stubs_.empty() || !is_running_; })) {
        return nullptr;
    }
    if (!is_running_) return nullptr;
    auto stub = std::move(stubs_.front());
    stubs_.pop();
    return stub;
}

void PushConnectPool::returnStub(std::unique_ptr<PushService::Stub> stub) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_running_) { stubs_.push(std::move(stub)); cond_.notify_one(); }
}

void PushConnectPool::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    is_running_ = false;
    cond_.notify_all();
    while (!stubs_.empty()) stubs_.pop();
}
