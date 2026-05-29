#ifndef GLOBAL_H
#define GLOBAL_H

#include <functional>
#include <memory>
#include <mutex>
#include <iostream>
#include <chrono>

#define CODE_PREFIX "code_"

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

struct PendingUserInfo {
    int uid;
    std::string username;
    std::string email;
    int role;
    int belong_team_id;
    int status = 0;
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
    USER_ALREADY_APPROVED = 1009
};

class Defer {
public:
    Defer(std::function<void()> func) : func_(func) {}
    ~Defer() { func_(); }
private:
    std::function<void()> func_;
};

#endif /* GLOBAL_H */
