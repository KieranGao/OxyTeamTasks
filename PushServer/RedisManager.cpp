#include "RedisManager.h"
#include "ConfigManager.h"
#include "Logger.h"

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

// 获取值，存入value中
bool RedisManager::get(const std::string& key, std::string& value) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) {
        return false;
    }
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "GET %s", key.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) {
        LOG_ERROR("Failed to execute GET command!");
        // 此时无需释放reply_，因为它是nullptr
        return false;
    }
    if (reply->type == REDIS_REPLY_STRING) {
        value = std::string(reply->str, reply->len);
        freeReplyObject(reply);
        LOG_DEBUG("Executed command [ GET {} ] success ! ", key);
        return true;
    }
    freeReplyObject(reply);
    return false;
}

bool RedisManager::set(const std::string& key, const std::string& value) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) {
        return false;
    }
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "SET %s %s", key.c_str(), value.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) {
        LOG_ERROR("Failed to execute SET command!");
        return false;
    }
    //如果执行失败则释放连接
    if (!(reply->type == REDIS_REPLY_STATUS and (strcmp(reply->str, "OK") == 0 or strcmp(reply->str, "ok") == 0)))
    {
        LOG_ERROR("Execute command [ SET {}  {} ] failure ! ", key, value);
        freeReplyObject(reply);     
        return false;
    }
    freeReplyObject(reply);
    LOG_DEBUG("Execut command [ SET {}  {} ] success ! ", key, value);
    return true;
}

bool RedisManager::auth(const std::string& password) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) {
        return false;
    }
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "AUTH %s", password.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) {
        LOG_ERROR("Failed to execute AUTH command!");
        return false;
    }

    if (reply and reply->type == REDIS_REPLY_ERROR) {
        LOG_ERROR("Failed to authenticate with Redis!");
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    LOG_DEBUG("Authenticated with Redis successfully.");
    return true;
}

bool RedisManager::del(const std::string& key) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) {
        return false;
    }
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "DEL %s", key.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) {
        LOG_ERROR("Failed to execute DEL command!");
        return false;
    }
    if (reply->type != REDIS_REPLY_INTEGER or reply->integer <= 0) {
        LOG_ERROR("Failed to execute DEL command!");
        freeReplyObject(reply);
        return false;
    }
    freeReplyObject(reply);
    LOG_DEBUG("Executed command [ DEL {} ] success ! ", key);
    return true;
}

bool RedisManager::lpush(const std::string& key, const std::string& value) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) {
        return false;
    }
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "LPUSH %s %s", key.c_str(), value.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) {
        LOG_ERROR("Failed to execute LPUSH command!");
        return false;
    }
    if (reply->type != REDIS_REPLY_INTEGER or reply->integer <= 0) {
        LOG_ERROR("Failed to execute LPUSH command!");
        freeReplyObject(reply);
        return false;
    }
    freeReplyObject(reply);
    LOG_DEBUG("Executed command [ LPUSH {}  {} ] success ! ", key, value);
    return true;
}

bool RedisManager::rpush(const std::string& key, const std::string& value) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) {
        return false;
    }
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "RPUSH %s %s", key.c_str(), value.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) {
        LOG_ERROR("Failed to execute RPUSH command!");
        return false;
    }
    if (reply->type != REDIS_REPLY_INTEGER or reply->integer <= 0) {
        LOG_ERROR("Failed to execute RPUSH command!");
        freeReplyObject(reply);
        return false;
    }
    freeReplyObject(reply);
    LOG_DEBUG("Executed command [ RPUSH {}  {} ] success ! ", key, value);
    return true;
}

bool RedisManager::lpop(const std::string& key, std::string& value) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) {
        return false;
    }
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "LPOP %s", key.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) {
        LOG_ERROR("Failed to execute LPOP command!");
        return false;
    }
    if (reply->type == REDIS_REPLY_STRING) {
        value = std::string(reply->str, reply->len);
        freeReplyObject(reply);
        LOG_DEBUG("Executed command [ LPOP {} ] success ! ", key);
        return true;
    }
    freeReplyObject(reply);
    LOG_ERROR("Failed to execute LPOP command!");
    return false;
}

bool RedisManager::rpop(const std::string& key, std::string& value) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) {
        return false;
    }
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "RPOP %s", key.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) {
        LOG_ERROR("Failed to execute RPOP command!");
        return false;
    }
    if (reply->type == REDIS_REPLY_STRING) {
        value = std::string(reply->str, reply->len);
        freeReplyObject(reply);
        LOG_DEBUG("Executed command [ RPOP {} ] success ! ", key);
        return true;
    }
    freeReplyObject(reply);
    LOG_ERROR("Failed to execute RPOP command!");
    return false;
}

bool RedisManager::hset(const std::string &key, const std::string &field, const std::string &value) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) {
        return false;
    }
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "HSET %s %s %s", key.c_str(), field.c_str(), value.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr or reply->type != REDIS_REPLY_INTEGER ) {
        LOG_ERROR("Executed command [ HSet {}  {}  {} ] failure ! ", key, field, value);
        freeReplyObject(reply);
        return false;
    }
    LOG_DEBUG("Executed command [ HSet {}  {}  {} ] success ! ", key, field, value);
    freeReplyObject(reply);
    return true;
}
bool RedisManager::hset(const char* key, const char* field, const char* value, size_t valuelen)
{
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) {
        return false;
    }
    const char* argv[4];
    size_t argvlen[4];
    argv[0] = "HSET";
    argvlen[0] = 4;
    argv[1] = key;
    argvlen[1] = strlen(key);
    argv[2] = field;
    argvlen[2] = strlen(field);
    argv[3] = value;
    argvlen[3] = valuelen;
    redisReply* reply = (redisReply*)redisCommandArgv(connect.get(), 4, argv, argvlen);
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr or reply->type != REDIS_REPLY_INTEGER) {
        LOG_ERROR("Executed command [ HSet {}  {}  {} ] failure ! ", key, field, value);
        freeReplyObject(reply);
        return false;
    }
    LOG_DEBUG("Executed command [ HSet {}  {}  {} ] success ! ", key, field, value);
    freeReplyObject(reply);
    return true;
}

std::string RedisManager::hget(const std::string &key, const std::string &field)
{
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) {
        return "";
    }
    const char* argv[3];
    size_t argvlen[3];
    argv[0] = "HGET";
    argvlen[0] = 4;
    argv[1] = key.c_str();
    argvlen[1] = key.length();
    argv[2] = field.c_str();
    argvlen[2] = field.length();
    redisReply* reply = static_cast<redisReply*>(redisCommandArgv(connect.get(), 3, argv, argvlen));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr || reply->type == REDIS_REPLY_NIL) {
        freeReplyObject(reply);
        LOG_ERROR("Executed command [ HGet {} {}  ] failure ! ", key, field);
        return "";
    }
    std::string value = reply->str;
    freeReplyObject(reply);
    LOG_DEBUG("Executed command [ HGet {} {} ] success ! ", key, field);
    return value;
}

bool RedisManager::existskey(const std::string& key) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) {
        return false;
    }
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "EXISTS %s", key.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) {
        LOG_ERROR("Failed to execute EXISTS command!");
        return false;
    }
    bool exists = (reply->type == REDIS_REPLY_INTEGER and reply->integer > 0);
    freeReplyObject(reply);
    LOG_DEBUG("Executed command [ EXISTS {} ] {} ! ", key, (exists ? "exists" : "does not exist"));
    return exists;
}

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
    LOG_DEBUG("Executed command [ SETEX {} {} {} ] success !", key, seconds, value);
    return true;
}

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

bool RedisManager::ltrim(const std::string& key, int start, int stop) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) return false;
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "LTRIM %s %d %d", key.c_str(), start, stop));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) { LOG_ERROR("Failed to execute LTRIM command!"); return false; }
    freeReplyObject(reply);
    return true;
}

bool RedisManager::lrange(const std::string& key, int start, int stop, std::vector<std::string>& values) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) return false;
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "LRANGE %s %d %d", key.c_str(), start, stop));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr || reply->type != REDIS_REPLY_ARRAY) { freeReplyObject(reply); return false; }
    for (size_t i = 0; i < reply->elements; ++i) {
        values.emplace_back(reply->element[i]->str, reply->element[i]->len);
    }
    freeReplyObject(reply);
    return true;
}

bool RedisManager::acquireLock(const std::string& lock_key, const std::string& owner_id, int ttl_seconds) {
    auto connect = conn_pool_->getConnection();
    if (connect == nullptr) return false;
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(),
        "SET %s %s NX EX %d", lock_key.c_str(), owner_id.c_str(), ttl_seconds));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) {
        LOG_ERROR("Failed to execute SET NX EX for lock: {}", lock_key);
        return false;
    }
    bool ok = (reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "OK") == 0);
    freeReplyObject(reply);
    if (ok) {
        LOG_DEBUG("Lock acquired: {} owner={}", lock_key, owner_id);
    }
    return ok;
}

bool RedisManager::releaseLock(const std::string& lock_key, const std::string& owner_id) {
    auto connect = conn_pool_->getConnection();
    if (connect == nullptr) return false;
    const char* lua = "if redis.call('GET',KEYS[1])==ARGV[1] then return redis.call('DEL',KEYS[1]) else return 0 end";
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(),
        "EVAL %s 1 %s %s", lua, lock_key.c_str(), owner_id.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) {
        LOG_ERROR("Failed to execute EVAL for releaseLock: {}", lock_key);
        return false;
    }
    bool released = (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    freeReplyObject(reply);
    if (released) {
        LOG_DEBUG("Lock released: {} owner={}", lock_key, owner_id);
    }
    return released;
}

bool RedisManager::acquireLockWithRetry(const std::string& lock_key, const std::string& owner_id,
                                         int ttl_seconds, int max_retries, int base_delay_ms) {
    for (int i = 0; i <= max_retries; ++i) {
        if (acquireLock(lock_key, owner_id, ttl_seconds)) {
            return true;
        }
        if (i < max_retries) {
            int delay = base_delay_ms * (1 << i);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
    }
    LOG_WARN("Lock acquisition failed after {} retries: {}", max_retries, lock_key);
    return false;
}

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
    if (reply == nullptr) {
        LOG_ERROR("Failed to execute EVAL script");
        return -1;
    }
    long long result = 0;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else if (reply->type == REDIS_REPLY_STRING) {
        try { result = std::stoll(reply->str); } catch (...) { result = -1; }
    } else if (reply->type == REDIS_REPLY_NIL) {
        result = 0;
    }
    freeReplyObject(reply);
    return result;
}

bool RedisManager::pushMessageAtomic(const std::string& uid_str, const std::string& msg_json,
                                      int max_messages, int ttl_seconds) {
    std::string lua =
        "local msg_key=KEYS[1] "
        "local counter_key=KEYS[2] "
        "local msg=ARGV[1] "
        "local max_msgs=tonumber(ARGV[2]) "
        "local ttl=tonumber(ARGV[3]) "
        "redis.call('LPUSH',msg_key,msg) "
        "redis.call('LTRIM',msg_key,0,max_msgs-1) "
        "redis.call('EXPIRE',msg_key,ttl) "
        "redis.call('INCR',counter_key) "
        "return 1";
    std::vector<std::string> keys = {"msgs:" + uid_str, "unread:" + uid_str};
    std::vector<std::string> args = {msg_json, std::to_string(max_messages), std::to_string(ttl_seconds)};
    long long result = evalScript(lua, keys, args);
    if (result < 0) {
        LOG_ERROR("pushMessageAtomic failed for uid={}", uid_str);
        return false;
    }
    LOG_DEBUG("pushMessageAtomic success for uid={}", uid_str);
    return true;
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
    std::vector<std::string> args = {
        std::to_string(decrement_count),
        std::to_string(ttl_seconds)
    };

    long long result = evalScript(lua, keys, args);
    if (result < 0) {
        LOG_ERROR("markReadAtomic failed for uid={}", uid_str);
        return false;
    }
    LOG_DEBUG("markReadAtomic success uid={} decr={}", uid_str, decrement_count);
    return true;
}


// 查看是否需要踢掉user，如果需要返回TRUE，并删除value
bool RedisManager::getAndDeleteKick(const std::string& uid_str, std::string& out_kick_value) {
    std::string lua =
        "local val=redis.call('GET',KEYS[1]) "
        "if val then redis.call('DEL',KEYS[1]) end "
        "return val";

    auto connect = conn_pool_->getConnection();
    if (connect == nullptr) return false;

    std::vector<std::string> parts = {"EVAL", lua, "1", "kick:" + uid_str};
    std::vector<const char*> argv(parts.size());
    std::vector<size_t> argvlen(parts.size());
    for (size_t i = 0; i < parts.size(); ++i) {
        argv[i] = parts[i].c_str();
        argvlen[i] = parts[i].size();
    }

    redisReply* reply = static_cast<redisReply*>(redisCommandArgv(
        connect.get(), static_cast<int>(parts.size()), argv.data(), argvlen.data()));
    conn_pool_->returnConnection(std::move(connect));

    if (reply == nullptr) {
        LOG_ERROR("getAndDeleteKick EVAL failed for uid={}", uid_str);
        return false;
    }
    if (reply->type == REDIS_REPLY_STRING) {
        out_kick_value = std::string(reply->str, reply->len);
        freeReplyObject(reply);
        LOG_DEBUG("getAndDeleteKick: found kick marker for uid={}", uid_str);
        return true;
    }
    freeReplyObject(reply);
    return false;
}


bool RedisManager::appendLogAtomic(const std::string& service_name, const std::string& log_json,
                                    int max_entries, int ttl_seconds) {
    std::string lua =
        "local key=KEYS[1] "
        "local entry=ARGV[1] "
        "local max_e=tonumber(ARGV[2]) "
        "local ttl=tonumber(ARGV[3]) "
        "redis.call('LPUSH',key,entry) " // 推入JSON日志信息
        "redis.call('LTRIM',key,0,max_e-1) " // 从左至右取最多max_e条日志
        "redis.call('EXPIRE',key,ttl) " // 续期KEY
        "return 1";
    std::vector<std::string> keys = {"logs:" + service_name};
    std::vector<std::string> args = {log_json, std::to_string(max_entries), std::to_string(ttl_seconds)};
    long long result = evalScript(lua, keys, args);
    if (result < 0) {
        LOG_ERROR("appendLogAtomic failed for service={}", service_name);
        return false;
    }
    return true;
}

void RedisManager::close() {
    conn_pool_->stop();
}