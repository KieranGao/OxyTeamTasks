#include "RedisConnectPool.h"

RedisConnectPool::RedisConnectPool(size_t pool_size, std::string host, int port, std::string password) :
    pool_size_(pool_size), host_(std::move(host)), port_(port), is_running_(true)   
    {   
        for(int i=1;i<=pool_size;i++) {
            redisContext* context = redisConnect(host_.c_str(), port);
            if(context == nullptr or context->err != 0) {
                std::cerr << "Create redis context failed!" << std::endl;
                if(context != nullptr) {
                    redisFree(context);
                }
                continue;
            }

            redisReply* reply = static_cast<redisReply*>(redisCommand(context, "AUTH %s", password.c_str()));
            if(reply->type == REDIS_REPLY_ERROR) {
                std::cerr << "AUTH FAILED!" << std::endl;
                freeReplyObject(reply);
                redisFree(context);
                continue;
            }
            std::cerr << "AUTH SUCCEED!" << std::endl;
            freeReplyObject(reply);
            // connections_.emplace(redisPtr{context, redisFree}); 函数指针型构造，需显式声明删除器
            connections_.emplace(context); // 仿函数形式则不用
        }
    }

RedisConnectPool::~RedisConnectPool() {
    stop();
    std::cerr << "RedisConnectPool Destoryed!" << std::endl;
}

redisPtr RedisConnectPool::getConnection() {
    std::unique_lock<std::mutex> lock(mutex_); 
    cond_.wait(lock, [this](){return !connections_.empty() or !is_running_;});
    if(!is_running_) {
        // 函数指针型不能直接返回nullptr
        // return redisPtr{nullptr, redisFree};
        // 仿函数型可以
        return nullptr;
    }
    auto context = std::move(connections_.front());
    connections_.pop();
    return context;
}

void RedisConnectPool::returnConnection(redisPtr connect) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(is_running_) {
        connections_.push(std::move(connect));
        cond_.notify_one();
    }
}   

void RedisConnectPool::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    is_running_.store(false);
    cond_.notify_all();
    while(!connections_.empty()) {
        connections_.pop();
    }
}