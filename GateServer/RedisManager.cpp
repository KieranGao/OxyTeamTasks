#include "RedisManager.h"
#include "ConfigManager.h"
#include "Logger.h"
#include <thread>
#include <chrono>

RedisManager::RedisManager() {
    auto& config = ConfigManager::getInstance();
    std::string host = config["Redis"]["host"];
    std::string port = config["Redis"]["port"];
    std::string password = config["Redis"]["password"];
    LOG_DEBUG("Redis connecting to {}:{}", host, port);
    conn_pool_ = std::make_unique<RedisConnectPool>(5, host, std::atoi(port.c_str()), password);
}

RedisManager::~RedisManager() {
    close(); // 关闭连接
}

// 设置键值对
bool RedisManager::set(const std::string& key, const std::string& value) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) return false;
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "SET %s %s", key.c_str(), value.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) { LOG_ERROR("Failed to execute SET command!"); return false; }
    if (!(reply->type == REDIS_REPLY_STATUS and (strcmp(reply->str, "OK") == 0 or strcmp(reply->str, "ok") == 0))) {
        freeReplyObject(reply); return false;
    }
    freeReplyObject(reply);
    return true;
}

// 获取值，存入value中
bool RedisManager::get(const std::string& key, std::string& value) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) return false;
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "GET %s", key.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) { LOG_ERROR("Failed to execute GET command!"); return false; }
    if (reply->type == REDIS_REPLY_STRING) {
        value = std::string(reply->str, reply->len);
        freeReplyObject(reply);
        return true;
    }
    freeReplyObject(reply);
    return false;
}

// 删除键
bool RedisManager::del(const std::string& key) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) return false;
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "DEL %s", key.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) { LOG_ERROR("Failed to execute DEL command!"); return false; }
    if (reply->type != REDIS_REPLY_INTEGER or reply->integer <= 0) {
        freeReplyObject(reply); return false;
    }
    freeReplyObject(reply);
    return true;
}

// Redis认证
bool RedisManager::auth(const std::string& password) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) return false;
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "AUTH %s", password.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) { LOG_ERROR("Failed to execute AUTH command!"); return false; }
    if (reply and reply->type == REDIS_REPLY_ERROR) {
        freeReplyObject(reply); return false;
    }
    freeReplyObject(reply);
    return true;
}

// 设置键值对并附带过期时间（秒）
bool RedisManager::setex(const std::string& key, const std::string& value, int seconds) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) return false;
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "SETEX %s %d %s", key.c_str(), seconds, value.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) { LOG_ERROR("Failed to execute SETEX command!"); return false; }
    if (!(reply->type == REDIS_REPLY_STATUS && (strcmp(reply->str, "OK") == 0 || strcmp(reply->str, "ok") == 0))) {
        freeReplyObject(reply); return false;
    }
    freeReplyObject(reply);
    return true;
}

// 设置键的过期时间（秒）
bool RedisManager::expire(const std::string& key, int seconds) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) return false;
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "EXPIRE %s %d", key.c_str(), seconds));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) { LOG_ERROR("Failed to execute EXPIRE command!"); return false; }
    bool ok = (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    freeReplyObject(reply);
    return ok;
}

// 原子自增
long RedisManager::incr(const std::string& key) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) return -1;
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "INCR %s", key.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) { freeReplyObject(reply); return -1; }
    long val = reply->integer;
    freeReplyObject(reply);
    return val;
}

// 原子自减
long RedisManager::decr(const std::string& key) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) return -1;
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "DECR %s", key.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr || reply->type != REDIS_REPLY_INTEGER) { freeReplyObject(reply); return -1; }
    long val = reply->integer;
    freeReplyObject(reply);
    return val;
}

// 分布式锁：SET NX EX 原子加锁
bool RedisManager::acquireLock(const std::string& lock_key, const std::string& owner_id, int ttl_seconds) {
    auto connect = conn_pool_->getConnection();
    if (connect == nullptr) return false;
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(),
        "SET %s %s NX EX %d", lock_key.c_str(), owner_id.c_str(), ttl_seconds));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) { LOG_ERROR("Failed to execute SET NX EX for lock: {}", lock_key); return false; }
    bool ok = (reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "OK") == 0);
    freeReplyObject(reply);
    if (ok) { LOG_DEBUG("Lock acquired: {} owner={}", lock_key, owner_id); }
    return ok;
}

// 分布式锁：Lua脚本原子释放锁（仅释放自己持有的锁）
bool RedisManager::releaseLock(const std::string& lock_key, const std::string& owner_id) {
    auto connect = conn_pool_->getConnection();
    if (connect == nullptr) return false;
    const char* lua = "if redis.call('GET',KEYS[1])==ARGV[1] then return redis.call('DEL',KEYS[1]) else return 0 end";
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(),
        "EVAL %s 1 %s %s", lua, lock_key.c_str(), owner_id.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) { LOG_ERROR("Failed to execute EVAL for releaseLock: {}", lock_key); return false; }
    bool released = (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    freeReplyObject(reply);
    if (released) { LOG_DEBUG("Lock released: {} owner={}", lock_key, owner_id); }
    return released;
}

// 分布式锁：带指数退避重试的加锁
bool RedisManager::acquireLockWithRetry(const std::string& lock_key, const std::string& owner_id,
                                         int ttl_seconds, int max_retries, int base_delay_ms) {
    for (int i = 0; i <= max_retries; ++i) {
        if (acquireLock(lock_key, owner_id, ttl_seconds)) return true;
        if (i < max_retries) {
            int delay = base_delay_ms * (1 << i);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
    }
    LOG_WARN("Lock acquisition failed after {} retries: {}", max_retries, lock_key);
    return false;
}

// Lua 脚本执行
long long RedisManager::evalScript(const std::string& lua_script,
                                    const std::vector<std::string>& keys,
                                    const std::vector<std::string>& args) {
    auto connect = conn_pool_->getConnection();
    if (connect == nullptr) return -1;
    std::vector<std::string> parts;
    parts.push_back("EVAL");
    parts.push_back(lua_script);
    parts.push_back(std::to_string(keys.size()));
    for (const auto& k : keys) parts.push_back(k);
    for (const auto& a : args) parts.push_back(a);
    std::vector<const char*> argv(parts.size());
    std::vector<size_t> argvlen(parts.size());
    for (size_t i = 0; i < parts.size(); ++i) {
        argv[i] = parts[i].c_str();
        argvlen[i] = parts[i].size();
    }
    redisReply* reply = static_cast<redisReply*>(redisCommandArgv(
        connect.get(), static_cast<int>(parts.size()), argv.data(), argvlen.data()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) { LOG_ERROR("Failed to execute EVAL script"); return -1; }
    long long result = 0;
    if (reply->type == REDIS_REPLY_INTEGER) result = reply->integer;
    else if (reply->type == REDIS_REPLY_STRING) { try { result = std::stoll(reply->str); } catch (...) { result = -1; } }
    else if (reply->type == REDIS_REPLY_NIL) result = 0;
    freeReplyObject(reply);
    return result;
}

// 减少用户未读消息数
bool RedisManager::markReadAtomic(const std::string& uid_str, int decrement_count, int ttl_seconds) {
    std::string lua =
        "local key = KEYS[1] "
        "local decr = tonumber(ARGV[1]) "
        "local ttl = tonumber(ARGV[2]) "
        // 等于0直接设置为0
        "if decr <= 0 then "
        "    redis.call('SETEX', key, ttl, 0) " // SETEX key ttl value 设置+过期（原子）
        "else "
        //DECRBY
        "    redis.call('DECRBY', key, decr) "
        "    redis.call('EXPIRE', key, ttl) " // EXPIRE 续期 REDIS KEY
        "end "
        "return 1";
    std::vector<std::string> keys = {"unread:" + uid_str};
    std::vector<std::string> args = {std::to_string(decrement_count), std::to_string(ttl_seconds)};
    long long result = evalScript(lua, keys, args);
    if (result < 0) {
        LOG_ERROR("markReadAtomic failed for uid={}", uid_str);
        return false;
    }
    LOG_DEBUG("markReadAtomic success uid={} decr={}", uid_str, decrement_count);
    return true;
}

void RedisManager::close() {
    conn_pool_->stop();
}
