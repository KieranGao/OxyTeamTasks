#include "RedisManager.h"
#include "ConfigManager.h"

RedisManager::RedisManager() {
    auto& config = ConfigManager::getInstance();
    std::string host = config["Redis"]["host"];
    std::string port = config["Redis"]["port"];
    std::string password = config["Redis"]["password"];
    std::cout << host << ' ' << port << ' ' << password << std::endl;
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
        std::cerr << "Failed to execute GET command!" << std::endl;
        // 此时无需释放reply_，因为它是nullptr
        return false;
    }
    if (reply->type == REDIS_REPLY_STRING) {
        value = std::string(reply->str, reply->len);
        freeReplyObject(reply);
        std::cerr << "Executed command [ GET " << key << " ] success ! " << std::endl;
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
        std::cerr << "Failed to execute SET command!" << std::endl;
        return false;
    }
    //如果执行失败则释放连接
    if (!(reply->type == REDIS_REPLY_STATUS and (strcmp(reply->str, "OK") == 0 or strcmp(reply->str, "ok") == 0)))
    {
        std::cerr << "Execute command [ SET " << key << "  " << value << " ] failure ! " << std::endl;
        freeReplyObject(reply);     
        return false;
    }
    freeReplyObject(reply);
    std::cerr << "Execut command [ SET " << key << "  " << value << " ] success ! " << std::endl;
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
        std::cerr << "Failed to execute AUTH command!" << std::endl;
        return false;
    }

    if (reply and reply->type == REDIS_REPLY_ERROR) {
        std::cerr << "Failed to authenticate with Redis!" << std::endl;
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    std::cerr << "Authenticated with Redis successfully." << std::endl;
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
        std::cerr << "Failed to execute DEL command!" << std::endl;
        return false;
    }
    if (reply->type != REDIS_REPLY_INTEGER or reply->integer <= 0) {
        std::cerr << "Failed to execute DEL command!" << std::endl;
        freeReplyObject(reply);
        return false;
    }
    freeReplyObject(reply);
    std::cerr << "Executed command [ DEL " << key << " ] success ! " << std::endl;
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
        std::cerr << "Failed to execute LPUSH command!" << std::endl;
        return false;
    }
    if (reply->type != REDIS_REPLY_INTEGER or reply->integer <= 0) {
        std::cerr << "Failed to execute LPUSH command!" << std::endl;
        freeReplyObject(reply);
        return false;
    }
    freeReplyObject(reply);
    std::cerr << "Executed command [ LPUSH " << key << "  " << value << " ] success ! " << std::endl;
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
        std::cerr << "Failed to execute RPUSH command!" << std::endl;
        return false;
    }
    if (reply->type != REDIS_REPLY_INTEGER or reply->integer <= 0) {
        std::cerr << "Failed to execute RPUSH command!" << std::endl;
        freeReplyObject(reply);
        std::cerr << "Executed command [ RPUSH " << key << "  " << value << " ] failure ! " << std::endl;
        return false;
    }
    freeReplyObject(reply);
    std::cerr << "Executed command [ RPUSH " << key << "  " << value << " ] success ! " << std::endl;
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
        std::cerr << "Failed to execute LPOP command!" << std::endl;
        return false;
    }
    if (reply->type == REDIS_REPLY_STRING) {
        value = std::string(reply->str, reply->len);
        freeReplyObject(reply);
        std::cerr << "Executed command [ LPOP " << key << " ] success ! " << std::endl;
        return true;
    }
    freeReplyObject(reply);
    std::cerr << "Failed to execute LPOP command!" << std::endl;
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
        std::cerr << "Failed to execute RPOP command!" << std::endl;
        return false;
    }
    if (reply->type == REDIS_REPLY_STRING) {
        value = std::string(reply->str, reply->len);
        freeReplyObject(reply);
        std::cerr << "Executed command [ RPOP " << key << " ] success ! " << std::endl;
        return true;
    }
    freeReplyObject(reply);
    std::cerr << "Failed to execute RPOP command!" << std::endl;
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
        std::cerr << "Executed command [ HSet " << key << "  " << field <<"  " << value << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        return false;
    }
    std::cerr << "Executed command [ HSet " << key << "  " << field <<"  " << value << " ] success ! " << std::endl;
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
        std::cerr << "Executed command [ HSet " << key << "  " << field << "  " << value << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        return false;
    }
    std::cerr << "Executed command [ HSet " << key << "  " << field << "  " << value << " ] success ! " << std::endl;
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
        std::cerr << "Executed command [ HGet " << key << " "<< field <<"  ] failure ! " << std::endl;
        return "";
    }
    std::string value = reply->str;
    freeReplyObject(reply);
    std::cerr << "Executed command [ HGet " << key << " " << field << " ] success ! " << std::endl;
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
        std::cerr << "Failed to execute EXISTS command!" << std::endl;
        return false;
    }
    bool exists = (reply->type == REDIS_REPLY_INTEGER and reply->integer > 0);
    freeReplyObject(reply);
    std::cerr << "Executed command [ EXISTS " << key << " ] " << (exists ? "exists" : "does not exist") << " ! " << std::endl;
    return exists;
}

void RedisManager::close() {
    conn_pool_->stop();
}