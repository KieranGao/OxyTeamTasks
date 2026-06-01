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
bool RedisManager::setex(const std::string& key, const std::string& value, int ttl_seconds) {
    auto connect = conn_pool_->getConnection();
    if(connect == nullptr) return false;
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(), "SETEX %s %d %s", key.c_str(), ttl_seconds, value.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) { LOG_ERROR("Failed to execute SETEX command!"); return false; }
    if (!(reply->type == REDIS_REPLY_STATUS and (strcmp(reply->str, "OK") == 0 or strcmp(reply->str, "ok") == 0))) {
        freeReplyObject(reply); return false;
    }
    freeReplyObject(reply);
    return true;
}

void RedisManager::close() {
    conn_pool_->stop();
}
