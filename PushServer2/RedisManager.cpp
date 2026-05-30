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

void RedisManager::close() {
    conn_pool_->stop();
}