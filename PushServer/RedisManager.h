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
    bool set(const std::string& key, const std::string& value);
    bool get(const std::string& key, std::string& value);
    bool del(const std::string& key);
    bool auth(const std::string& password);
    // 此处是redis中的双端队列操作
    bool lpush(const std::string& key, const std::string& value);
    bool rpush(const std::string& key, const std::string& value);
    bool lpop(const std::string& key, std::string& value);
    bool rpop(const std::string& key, std::string& value);
    bool existskey(const std::string& key);
    bool setex(const std::string& key, const std::string& value, int seconds);
    bool expire(const std::string& key, int seconds);
    long incr(const std::string& key); // 原子自增
    long decr(const std::string& key); // 原子自减
    bool ltrim(const std::string& key, int start, int stop);
    bool lrange(const std::string& key, int start, int stop, std::vector<std::string>& values);
    // hset和hget是redis中的hash表操作，hset可以设置一个key下的field和value，hget可以获取一个key下的field对应的value
    // 形象来说，key->field->value，就像一个二维表一样，key是表名，field是列名，value是数据
    bool hset(const std::string& key, const std::string& field, const std::string& value);
    bool hset(const char* key, const char* field, const char* value, size_t hvaluelen);
    std::string hget(const std::string &key, const std::string &field);
    // 分布式锁
    bool acquireLock(const std::string& lock_key, const std::string& owner_id, int ttl_seconds = 30);
    bool releaseLock(const std::string& lock_key, const std::string& owner_id);
    bool acquireLockWithRetry(const std::string& lock_key, const std::string& owner_id,
                              int ttl_seconds = 30, int max_retries = 3, int base_delay_ms = 50);

    // Lua 脚本执行
    long long evalScript(const std::string& lua_script,
                         const std::vector<std::string>& keys,
                         const std::vector<std::string>& args);

    // 原子消息推送: LPUSH + LTRIM + EXPIRE + INCR
    bool pushMessageAtomic(const std::string& uid_str, const std::string& msg_json,
                           int max_messages = 50, int ttl_seconds = 604800);
    // 原子标记已读: SETEX(0) 或 DECR N 次
    bool markReadAtomic(const std::string& uid_str, int decrement_count, int ttl_seconds = 604800);
    // 原子踢人标记: GET + DEL
    bool getAndDeleteKick(const std::string& uid_str, std::string& out_kick_value);
    // 原子日志追加: LPUSH + LTRIM + EXPIRE
    bool appendLogAtomic(const std::string& service_name, const std::string& log_json,
                         int max_entries = 500, int ttl_seconds = 604800);

    void close();
private:
    RedisManager();
    std::unique_ptr<RedisConnectPool> conn_pool_;
};


#endif /* REDISMANAGER_H */


/*

void TestRedisManager() {

    // assert(RedisManager::getInstance()->connect("127.0.0.1", 6379));
    assert(RedisManager::getInstance()->auth("123456"));
    assert(RedisManager::getInstance()->set("blogwebsite","KieranGao.github.io"));
    std::string value="";
    assert(RedisManager::getInstance()->get("blogwebsite", value) );
    assert(RedisManager::getInstance()->get("nonekey", value) == false);
    assert(RedisManager::getInstance()->hset("bloginfo","blogwebsite", "KieranGao.github.io"));
    assert(RedisManager::getInstance()->hget("bloginfo","blogwebsite") != "");
    assert(RedisManager::getInstance()->existskey("bloginfo"));
    assert(RedisManager::getInstance()->del("bloginfo"));
    assert(RedisManager::getInstance()->del("bloginfo") == false);
    assert(RedisManager::getInstance()->existskey("bloginfo") == false);
    assert(RedisManager::getInstance()->lpush("lpushkey1", "lpushvalue1"));
    assert(RedisManager::getInstance()->lpush("lpushkey1", "lpushvalue2"));
    assert(RedisManager::getInstance()->lpush("lpushkey1", "lpushvalue3"));
    assert(RedisManager::getInstance()->rpop("lpushkey1", value));
    assert(RedisManager::getInstance()->rpop("lpushkey1", value));
    assert(RedisManager::getInstance()->lpop("lpushkey1", value));
    assert(RedisManager::getInstance()->lpop("lpushkey2", value)==false);
    // RedisManager::getInstance()->close();
}

*/
