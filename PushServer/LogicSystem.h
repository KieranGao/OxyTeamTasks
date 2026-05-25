#ifndef LOGICSYSTEM_H
#define LOGICSYSTEM_H

#include "Singleton.h"
#include <queue>
#include <thread>
#include "Session.h"
#include <functional>
#include "Global.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include <unordered_map>
#include <atomic>
#include <string>

typedef std::function<void(std::shared_ptr<Session>, const std::string& msg_data)> FunCallBack;

class LogicSystem : public Singleton<LogicSystem> {
    friend class Singleton<LogicSystem>;
public:
    ~LogicSystem();
    void postMsgToQue(std::shared_ptr<Session> session, std::string msg_data);
private:
    LogicSystem();
    void dealMsg();
    void registerCallBacks();
    void loginHandler(std::shared_ptr<Session> session, const std::string& msg_data);

    struct LogicNode {
        std::shared_ptr<Session> session_;
        std::string msg_data_;
    };

    std::thread workers_thread_;
    std::queue<LogicNode> msg_queue_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> is_running_;
    std::unordered_map<std::string, FunCallBack> fun_callbacks_;
    std::unordered_map<int, std::shared_ptr<UserInfo>> users_;
};

#endif /* LOGICSYSTEM_H */
