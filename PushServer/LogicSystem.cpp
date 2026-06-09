#include "LogicSystem.h"
#include "AsyncTaskPool.h"
#include "StatusGrpcClient.h"
#include "MySQLManager.h"
#include "RedisManager.h"
#include "MainServer.h"
#include "Global.h"
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/random_generator.hpp"
#include "boost/uuid/uuid_io.hpp"
#include "Logger.h"
#include <sstream>

static std::string generate_lock_owner() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}

LogicSystem::LogicSystem() : is_running_(true) {
    registerCallBacks();
    unsigned int n = std::thread::hardware_concurrency();
    if (n < 2) n = 2;
    for (unsigned int i = 0; i < n; ++i) {
        workers_threads_.emplace_back(&LogicSystem::dealMsg, this);
    }
}

LogicSystem::~LogicSystem() {
    is_running_ = false;
    cv_.notify_all();
    for (auto& t : workers_threads_) {
        if (t.joinable()) t.join();
    }
}

void LogicSystem::postMsgToQue(std::shared_ptr<Session> session, std::string msg_data) {
    std::unique_lock<std::mutex> lock(mtx_);
    msg_queue_.push({session, std::move(msg_data)});
    if (msg_queue_.size() == 1) {
        lock.unlock();
        cv_.notify_one();
    }
}

void LogicSystem::dealMsg() {
    for (;;) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]() {
            return !msg_queue_.empty() || !is_running_.load();
        });
        if (!is_running_) break;

        while (!msg_queue_.empty()) {
            auto node = std::move(msg_queue_.front());
            msg_queue_.pop();
            lock.unlock();
            Json::Value root;
            Json::Reader reader;
            if (!reader.parse(node.msg_data_, root)) {
                LOG_ERROR("[LogicSystem] JSON parse error");
                lock.lock();
                continue;
            }
            std::string type = root.get("type", "").asString();
            LOG_DEBUG("[LogicSystem] msg type: {}", type);

            auto cb = fun_callbacks_.find(type);
            if (cb != fun_callbacks_.end()) {
                cb->second(node.session_, node.msg_data_);
            } else {
                LOG_DEBUG("[LogicSystem] no handler for type: {}", type);
            }
            lock.lock();
        }
    }
}

void LogicSystem::registerCallBacks() {
    fun_callbacks_[WS_MSG_LOGIN] = std::bind(&LogicSystem::loginHandler, this,
        std::placeholders::_1, std::placeholders::_2);
}

void LogicSystem::loginHandler(std::shared_ptr<Session> session, const std::string& msg_data) {
    Json::Reader reader;
    Json::Value root;
    if (!reader.parse(msg_data, root)) {
        LOG_ERROR("[PushServer] loginHandler: JSON parse failed");
        Json::Value err;
        err["type"] = WS_MSG_LOGIN_RSP;
        err["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
        session->send(err.toStyledString());
        return;
    }
    int uid = root["uid"].asInt();
    std::string token = root["token"].asString();
    if (uid <= 0 or token.empty()) {
        LOG_ERROR("[PushServer] loginHandler: invalid uid or token");
        Json::Value err;
        err["type"] = WS_MSG_LOGIN_RSP;
        err["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
        session->send(err.toStyledString());
        return;
    }
    LOG_DEBUG("[PushServer] login: uid={} token={}", uid, token);
    Json::Value rtvalue;
    rtvalue["type"] = WS_MSG_LOGIN_RSP;
    Defer defer([&rtvalue, session]() {
        session->send(rtvalue.toStyledString());
        if (rtvalue["error"].asInt() != 0) {
            session->close();
        }
    });

    LoginReportRsp rsp = StatusGrpcClient::getInstance().reportLogin(uid, token, session->getServer()->getServerName());
    if (rsp.error() != 0) {
        LOG_ERROR("[PushServer] loginHandler: token validation failed for uid={}, error={}", uid, rsp.error());
        rtvalue["error"] = rsp.error();
        return;
    }
    rtvalue["error"] = 0;
    rtvalue["uid"] = uid;

    // 旁路缓存：先查 Redis，miss 再查 MySQL 并写回 Redis
    std::string uid_str = std::to_string(uid);
    std::shared_ptr<UserInfo> user_info;
    std::string cache_key = "user_info:" + uid_str;
    std::string cached;
    if (RedisManager::getInstance().get(cache_key, cached) && !cached.empty()) {
        // Redis 命中，解析 "username|email|role|captain_id|team_id"
        user_info = std::make_shared<UserInfo>();
        user_info->uid = uid;
        std::istringstream ss(cached);
        std::string field;
        std::getline(ss, user_info->username, '|');
        std::getline(ss, user_info->email, '|');
        if (std::getline(ss, field, '|')) user_info->role = std::stoi(field);
        if (std::getline(ss, field, '|')) user_info->belong_captain_id = std::stoi(field);
        if (std::getline(ss, field, '|')) user_info->belong_team_id = std::stoi(field);
    } else {
        // Redis miss，查 MySQL 并写回 Redis
        user_info = MySQLManager::getInstance().getUser(uid);
        if (!user_info) {
            rtvalue["error"] = static_cast<int>(ErrorCodes::USER_ID_INVALID);
            return;
        }
        std::string val = user_info->username + "|" + user_info->email + "|"
            + std::to_string(user_info->role) + "|"
            + std::to_string(user_info->belong_captain_id) + "|"
            + std::to_string(user_info->belong_team_id);
        RedisManager::getInstance().set(cache_key, val);  // 不设 TTL，靠写穿删除
    }
    rtvalue["name"] = user_info->username;

    // 若当前用户已经在线，需要将旧会话踢下线

    // 原子踢人标记检查: 单次Lua调用 GET + DEL
    std::string kick_val;
    if (RedisManager::getInstance().getAndDeleteKick(uid_str, kick_val)) {
        auto oldSession = session->getServer()->getSessionByUid(uid);
        if (oldSession) {
            LOG_INFO("[PushServer] Kicking old session for uid={}", uid);
            Json::Value kickMsg;
            kickMsg["type"] = WS_MSG_KICKED;
            kickMsg["reason"] = "other_login";
            oldSession->send(kickMsg.toStyledString());
            oldSession->close();
            session->getServer()->removeUidSession(uid);
        }
    }

    session->setUid(uid);
    session->getServer()->addUidSession(uid, session);

    // 未读计数：优先用 MySQL 校准 Redis（防止 Redis 重启后计数归零）
    long unread_count = MySQLManager::getInstance().getUnreadCount(uid);
    RedisManager::getInstance().set("unread:" + uid_str, std::to_string(unread_count));
    RedisManager::getInstance().expire("unread:" + uid_str, 604800);
    rtvalue["unread_count"] = static_cast<Json::Int64>(unread_count);

    // 推送未读消息
    std::vector<std::string> cached_msgs;
    if (RedisManager::getInstance().lrange("msgs:" + uid_str, 0, 19, cached_msgs)) {
        Json::Value offline(Json::arrayValue);
        for (const auto& msg_json : cached_msgs) {
            Json::Value msg;
            Json::Reader reader;
            if (reader.parse(msg_json, msg)) {
                offline.append(msg);
            }
        }
        rtvalue["offline_messages"] = offline;
    }

    LOG_INFO("[PushServer] login success: uid={} name={} unread={}", uid, user_info->username, unread_count);
    // 后续会由defer的回调去发送JSON回包
}
