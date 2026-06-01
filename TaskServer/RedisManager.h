#ifndef REDISMANAGER_H
#define REDISMANAGER_H

#include <hiredis/hiredis.h>
#include "Singleton.h"
#include "Global.h"
#include "RedisConnectPool.h"
class RedisManager : public Singleton<RedisManager> {

    friend class Singleton<RedisManager>;
public:
    ~RedisManager();
    // 设置键值对
    bool set(const std::string& key, const std::string& value);
    // 获取值，存入value中
    bool get(const std::string& key, std::string& value);
    // 删除键
    bool del(const std::string& key);
    // Redis认证
    bool auth(const std::string& password);

    void close();
private:
    RedisManager();
    std::unique_ptr<RedisConnectPool> conn_pool_;
};


#endif /* REDISMANAGER_H */
