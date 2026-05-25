#include "LogicSystem.h"
#include "StatusGrpcClient.h"
#include "MySQLManager.h"
#include "Global.h"

LogicSystem::LogicSystem() : is_running_(true) {
    registerCallBacks();
    workers_thread_ = std::thread(&LogicSystem::dealMsg, this);
}

LogicSystem::~LogicSystem() {
    is_running_ = false;
    cv_.notify_one();
    workers_thread_.join();
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

            // Parse JSON to extract message type
            Json::Value root;
            Json::Reader reader;
            if (!reader.parse(node.msg_data_, root)) {
                std::cerr << "[LogicSystem] JSON parse error" << std::endl;
                lock.lock();
                continue;
            }
            std::string type = root.get("type", "").asString();
            std::cout << "[LogicSystem] msg type: " << type << std::endl;

            auto cb = fun_callbacks_.find(type);
            if (cb != fun_callbacks_.end()) {
                cb->second(node.session_, node.msg_data_);
            } else {
                std::cout << "[LogicSystem] no handler for type: " << type << std::endl;
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
    reader.parse(msg_data, root);
    int uid = root["uid"].asInt();
    std::string token = root["token"].asString();
    std::cout << "[PushServer] TCP login: uid=" << uid << " token=" << token << std::endl;

    // Validate token via StatusServer
    LoginReportRsp rsp = StatusGrpcClient::getInstance().reportLogin(uid, token);

    Json::Value rtvalue;
    Defer defer([this, &rtvalue, session]() {
        std::string return_str = rtvalue.toStyledString();
        session->send(return_str);
    });

    rtvalue["type"] = WS_MSG_LOGIN_RSP;
    rtvalue["error"] = static_cast<int>(rsp.error());

    if (rsp.error() != static_cast<int>(ErrorCodes::SUCCESS)) {
        return;
    }

    // Lookup or cache user info
    auto it = users_.find(uid);
    std::shared_ptr<UserInfo> user_info = nullptr;
    if (it == users_.end()) {
        user_info = MySQLManager::getInstance().getUser(uid);
        if (!user_info) {
            rtvalue["error"] = static_cast<int>(ErrorCodes::USER_ID_INVALID);
            return;
        }
        users_[uid] = user_info;
    } else {
        user_info = it->second;
    }

    rtvalue["uid"] = uid;
    rtvalue["name"] = user_info->username;
}
