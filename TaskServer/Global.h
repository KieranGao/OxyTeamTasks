#ifndef GLOBAL_H
#define GLOBAL_H

#include <functional>
#include <memory>
#include <mutex>
#include <iostream>
#include <chrono>

#define CODE_PREFIX "code_"

struct AssigneeStatus {
    int assignee_uid;
    int status;
};

struct TaskInfo {
    int id;
    int uid;
    std::string title;
    std::string description;
    int status;
    int priority;
    std::string deadline;
    std::string assigned_to;
    std::string created_at;
    std::string updated_at;
    int my_status;
    std::vector<AssigneeStatus> assignee_statuses;
};

struct TodoInfo {
    int id;
    int uid;
    std::string content;
    int priority;
    std::string deadline;
    int is_finished;
};

struct CheckinRecord {
    int uid;
    std::string checkin_date;
    std::string created_at;
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
    TASK_NOT_FOUND = 2001,
    TODO_NOT_FOUND = 2002,
    CHECKIN_ALREADY_DONE = 3001,
};

class Defer {
public:
    Defer(std::function<void()> func) : func_(func) {}
    ~Defer() { func_(); }
private:
    std::function<void()> func_;
};

#endif /* GLOBAL_H */
