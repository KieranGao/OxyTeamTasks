#ifndef GLOBAL_H
#define GLOBAL_H

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <mutex>
#include <iostream>
#include <json/json.h>

#define CODE_PREFIX "code_"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

struct UserInfo {
    std::string username;
    std::string password;
    std::string email;
    int uid;
    int role;
    int belong_captain_id;
    int belong_team_id;
    int status;
};

enum class ErrorCodes {
    SUCCESS = 0,
    JSON_PARSE_ERROR = 1001,
    RPC_ERROR = 1002,
    VERIFY_CODE_EXPIRED = 1003,
    USER_ALREADY_EXISTS = 1004,
    USER_DO_NOT_EXISTS = 1005,
    USER_LOGIN_ERROR = 1006,
    USER_ID_INVALID = 1007,
    USER_NOT_APPROVED = 1008,
    USER_ALREADY_APPROVED = 1009,
    INVALID_TOKEN = 1010
};

// WEBSOCKET MESSAGE TYPE
#define WS_MSG_LOGIN       "login"
#define WS_MSG_LOGIN_RSP   "login_rsp"
#define WS_MSG_NOTIFY      "notify"
#define WS_MSG_TASK_NEW    "task_new"
#define WS_MSG_TASK_UPDATE "task_update"
#define WS_MSG_TASK_DONE   "task_done" 
#define WS_MSG_TASK_REMIND "task_remind" // 任务提醒
#define WS_MSG_CHECKIN_REMIND "checkin_remind" // 打卡提醒
#define WS_MSG_KICKED      "kicked" // 重复登录踢掉旧会话

#define MAX_SENDQUE 1000

class Defer {
public:
    Defer(std::function<void()> func) : func_(func) {}
    ~Defer() { func_(); }
private:
    std::function<void()> func_;
};

#endif /* GLOBAL_H */
