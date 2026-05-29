#ifndef REDISCONNECTPOOL_H
#define REDISCONNECTPOOL_H

#include <hiredis/hiredis.h>
#include <queue>
#include <mutex>
#include <string>
#include <iostream>
#include <condition_variable>
#include <memory>
#include <atomic>

// 仿函数形式的自定义删除器，无函数指针内存开销
struct redisDel {
    void operator()(redisContext* ptr) const {
        if(ptr) {
            redisFree(ptr);
        }
    }
};
// using redisPtr = std::unique_ptr<redisContext, decltype(&redisFree)>; 函数指针型，存在函数指针的内存额外开销
using redisPtr = std::unique_ptr<redisContext, redisDel>;
class RedisConnectPool {
public:
    RedisConnectPool(size_t pool_size, std::string host, int port, std::string password);
    ~RedisConnectPool();
    redisPtr getConnection();
    void returnConnection(redisPtr connect);
    void stop();
private:
    std::mutex mutex_;
    std::queue<redisPtr> connections_;
    std::condition_variable cond_;
    std::atomic<bool> is_running_;
    std::string host_;
    size_t pool_size_;
    int port_;
};

#endif /* REDISCONNECTPOOL_H */
