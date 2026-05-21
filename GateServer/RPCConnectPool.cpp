#include "RPCConnectPool.h"

VerifyConnectPool::VerifyConnectPool(size_t pool_size, std::string host, std::string port) 
    : is_running_(true), host_(std::move(host)), port_(std::move(port)), pool_size_(pool_size) 
{
    for(int i=0;i<pool_size;i++) {
        std::shared_ptr<Channel> channel = grpc::CreateChannel(host_ + ":" + port_, grpc::InsecureChannelCredentials());
        stubs_.emplace(VerifyService::NewStub(channel));
    }
}

VerifyConnectPool::~VerifyConnectPool() {
    // std::lock_guard<std::mutex> lock(mutex_); 不可加，会产生死锁
    stop();
    std::cerr << "VerifyConnectPool is destroyed" << std::endl;
}

std::unique_ptr<VerifyService::Stub> VerifyConnectPool::getStub() {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this](){ return !stubs_.empty() or !is_running_; });
    if(!is_running_) {
        return nullptr; // 连接池已停止，返回空指针
    }
    auto stub = std::move(stubs_.front());
    stubs_.pop();
    return stub;
}

void VerifyConnectPool::returnStub(std::unique_ptr<VerifyService::Stub> stub) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(is_running_) {
        stubs_.push(std::move(stub));
        cond_.notify_one();
    }
}

void VerifyConnectPool::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    is_running_ = false;
    cond_.notify_all(); // 唤醒所有等待的线程，让它们退出
    while(!stubs_.empty()) {
        // stubs_.front().reset(); pop时自动调用
        stubs_.pop();
    }
}


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
    std::cerr << "StatusConnectPool is destroyed" << std::endl;
}

std::unique_ptr<StatusService::Stub> StatusConnectPool::getStub() {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this](){ return !stubs_.empty() or !is_running_; });
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
    cond_.notify_all(); // 唤醒所有等待的线程，让它们退出
    while(!stubs_.empty()) {
        // stubs_.front().reset(); pop时自动调用
        stubs_.pop();
    }
}