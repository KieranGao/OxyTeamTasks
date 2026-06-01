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
    // 设置键值对并附带过期时间（秒）
    bool setex(const std::string& key, const std::string& value, int seconds);
    // 设置键的过期时间（秒）
    bool expire(const std::string& key, int seconds);
    // 原子自增
    long incr(const std::string& key);
    // 原子自减
    long decr(const std::string& key);

    // 分布式锁：SET NX EX 原子加锁
    bool acquireLock(const std::string& lock_key, const std::string& owner_id, int ttl_seconds = 30);
    // 分布式锁：Lua脚本原子释放锁（仅释放自己持有的锁）
    bool releaseLock(const std::string& lock_key, const std::string& owner_id);
    // 分布式锁：带指数退避重试的加锁
    bool acquireLockWithRetry(const std::string& lock_key, const std::string& owner_id,
                              int ttl_seconds = 30, int max_retries = 3, int base_delay_ms = 50);

    // Lua 脚本执行
    long long evalScript(const std::string& lua_script,
                         const std::vector<std::string>& keys,
                         const std::vector<std::string>& args);

    // 原子标记已读: DECRBY 减少未读计数，SETEX(0) 清零
    bool markReadAtomic(const std::string& uid_str, int decrement_count, int ttl_seconds = 604800);

    void close();
private:
    RedisManager();
    std::unique_ptr<RedisConnectPool> conn_pool_;
};


#endif /* REDISMANAGER_H */
