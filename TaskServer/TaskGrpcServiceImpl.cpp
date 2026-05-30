#include "TaskGrpcServiceImpl.h"
#include "MySQLManager.h"
#include "PushGrpcClient.h"
#include "Logger.h"
#include <thread>
#include <sstream>

// parse "1,3,5" assigned_to string into vector<int>
static std::vector<int> parseAssignedTo(const std::string& assigned_to) {
    std::vector<int> uids;
    if (assigned_to.empty() || assigned_to == "0") return uids;
    std::istringstream ss(assigned_to);
    std::string token;
    while (std::getline(ss, token, ',')) {
        try { uids.push_back(std::stoi(token)); } catch (...) {}
    }
    return uids;
}

// build JSON payload for push messages
static std::string buildPayload(int taskId, const std::string& title) {
    return "{\"task_id\":" + std::to_string(taskId) + ",\"task_title\":\"" + title + "\"}";
}

TaskGrpcServiceImpl::TaskGrpcServiceImpl() {}

Status TaskGrpcServiceImpl::CreateTask(ServerContext* context, const CreateTaskReq* req, CreateTaskRsp* resp) {
    int uid = req->uid();
    std::string title = req->title();
    std::string description = req->description();
    int priority = req->priority() > 0 ? req->priority() : 3;
    std::string deadline = req->deadline();
    std::string assigned_to = req->assigned_to();

    LOG_INFO("[Task] CreateTask uid={} title=\"{}\"", uid, title);

    int id = MySQLManager::getInstance().createTask(uid, title, description, priority, deadline, assigned_to);
    if (id <= 0) {
        resp->set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
        return Status::OK;
    }
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    resp->set_id(id);

    // 异步通知
    std::thread([assigned_to, title, id]() {
        auto uids = parseAssignedTo(assigned_to);
        std::string payload = buildPayload(id, title);
        for (int assignee_uid : uids) {
            PushGrpcClient::getInstance().pushToUser(assignee_uid, "task_new", "新任务指派: " + title, payload);
        }
    }).detach();

    return Status::OK;
}

Status TaskGrpcServiceImpl::UpdateTask(ServerContext* context, const UpdateTaskReq* req, UpdateTaskRsp* resp) {
    int id = req->id();
    int uid = req->uid();

    LOG_INFO("[Task] UpdateTask id={} uid={}", id, uid);

    int old_assignee_status = -1;
    if (uid > 0) {
        old_assignee_status = MySQLManager::getInstance().getAssigneeStatus(id, uid);
    }

    bool ok = MySQLManager::getInstance().updateTask(
        id, uid, req->title(), req->description(), req->status(),
        req->priority(), req->deadline(), req->assigned_to());
    if (!ok) {
        resp->set_error(static_cast<int>(ErrorCodes::TASK_NOT_FOUND));
        return Status::OK;
    }
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));

    int task_uid = req->uid();      // 0=global, >0=per-assignee
    int status = req->status();
    std::string assigned_to = req->assigned_to();
    std::string title = req->title();
    std::thread([id, task_uid, status, assigned_to, title, old_assignee_status]() {
        std::string payload = buildPayload(id, title);
        if (task_uid == 0) {
            auto uids = parseAssignedTo(assigned_to);
            for (int assignee_uid : uids) {
                PushGrpcClient::getInstance().pushToUser(assignee_uid, "task_update", "任务更新: " + title, payload);
            }
        } else if (status == 2) {
            // completed task → notify creator
            TaskInfo taskInfo;
            if (MySQLManager::getInstance().getTask(id, taskInfo) && taskInfo.uid > 0) {
                PushGrpcClient::getInstance().pushToUser(taskInfo.uid, "task_done", "任务已完成: " + title, payload);
            }
        } else if (status == 1 && old_assignee_status == 2) {
            // （打回操作）已完成 -> 进行中
            PushGrpcClient::getInstance().pushToUser(task_uid, "task_update", "任务被打回: " + title, payload);
        }
    }).detach();

    return Status::OK;
}

Status TaskGrpcServiceImpl::DeleteTask(ServerContext* context, const DeleteTaskReq* req, DeleteTaskRsp* resp) {
    int id = req->id();
    int uid = req->uid();

    LOG_INFO("[Task] DeleteTask id={} uid={}", id, uid);

    bool ok = MySQLManager::getInstance().deleteTask(id, uid);
    if (!ok) {
        resp->set_error(static_cast<int>(ErrorCodes::TASK_NOT_FOUND));
        return Status::OK;
    }
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    return Status::OK;
}

Status TaskGrpcServiceImpl::GetTask(ServerContext* context, const GetTaskReq* req, GetTaskRsp* resp) {
    int id = req->id();

    LOG_INFO("[Task] GetTask id={}", id);

    TaskInfo info;
    bool ok = MySQLManager::getInstance().getTask(id, info);
    if (!ok) {
        resp->set_error(static_cast<int>(ErrorCodes::TASK_NOT_FOUND));
        return Status::OK;
    }
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    auto* task = resp->mutable_task();
    task->set_id(info.id);
    task->set_uid(info.uid);
    task->set_title(info.title);
    task->set_description(info.description);
    task->set_status(info.status);
    task->set_priority(info.priority);
    task->set_deadline(info.deadline);
    task->set_assigned_to(info.assigned_to);
    task->set_created_at(info.created_at);
    task->set_updated_at(info.updated_at);
    task->set_my_status(info.my_status);
    for (auto& as : info.assignee_statuses) {
        auto* pas = task->add_assignee_statuses();
        pas->set_assignee_uid(as.assignee_uid);
        pas->set_status(as.status);
    }
    return Status::OK;
}

Status TaskGrpcServiceImpl::ListTasks(ServerContext* context, const ListTasksReq* req, ListTasksRsp* resp) {
    int uid = req->uid();
    int status = req->status();
    std::string assigned_to = req->assigned_to();

    LOG_INFO("[Task] ListTasks uid={} status={} assigned_to={}", uid, status, assigned_to);

    std::vector<TaskInfo> tasks;
    bool ok = MySQLManager::getInstance().listTasks(uid, status, assigned_to, tasks);
    if (!ok) {
        resp->set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
        return Status::OK;
    }
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    for (auto& t : tasks) {
        auto* task = resp->add_tasks();
        task->set_id(t.id);
        task->set_uid(t.uid);
        task->set_title(t.title);
        task->set_description(t.description);
        task->set_status(t.status);
        task->set_priority(t.priority);
        task->set_deadline(t.deadline);
        task->set_assigned_to(t.assigned_to);
        task->set_created_at(t.created_at);
        task->set_updated_at(t.updated_at);
        task->set_my_status(t.my_status);
        for (auto& as : t.assignee_statuses) {
            auto* pas = task->add_assignee_statuses();
            pas->set_assignee_uid(as.assignee_uid);
            pas->set_status(as.status);
        }
    }
    return Status::OK;
}

Status TaskGrpcServiceImpl::AddTodo(ServerContext* context, const AddTodoReq* req, AddTodoRsp* resp) {
    int uid = req->uid();
    std::string content = req->content();
    int priority = req->priority() > 0 ? req->priority() : 3;
    std::string deadline = req->deadline();

    LOG_INFO("[Task] AddTodo uid={} content=\"{}\"", uid, content);

    int id = MySQLManager::getInstance().addTodo(uid, content, priority, deadline);
    if (id <= 0) {
        resp->set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
        return Status::OK;
    }
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    resp->set_id(id);
    return Status::OK;
}

Status TaskGrpcServiceImpl::ListTodo(ServerContext* context, const ListTodoReq* req, ListTodoRsp* resp) {
    int uid = req->uid();
    int is_finished = req->is_finished();

    LOG_INFO("[Task] ListTodo uid={} is_finished={}", uid, is_finished);

    std::vector<TodoInfo> todos;
    bool ok = MySQLManager::getInstance().listTodo(uid, is_finished, todos);
    if (!ok) {
        resp->set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
        return Status::OK;
    }
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    for (auto& t : todos) {
        auto* todo = resp->add_todos();
        todo->set_id(t.id);
        todo->set_uid(t.uid);
        todo->set_content(t.content);
        todo->set_priority(t.priority);
        todo->set_deadline(t.deadline);
        todo->set_is_finished(t.is_finished);
    }
    return Status::OK;
}

Status TaskGrpcServiceImpl::UpdateTodo(ServerContext* context, const UpdateTodoReq* req, UpdateTodoRsp* resp) {
    int id = req->id();
    int uid = req->uid();
    std::string content = req->content();
    int priority = req->priority() > 0 ? req->priority() : 3;
    std::string deadline = req->deadline();
    int is_finished = req->is_finished();

    LOG_INFO("[Task] UpdateTodo id={} uid={} is_finished={}", id, uid, is_finished);

    bool ok = MySQLManager::getInstance().updateTodo(id, uid, content, priority, deadline, is_finished);
    if (!ok) {
        resp->set_error(static_cast<int>(ErrorCodes::TODO_NOT_FOUND));
        return Status::OK;
    }
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    return Status::OK;
}

Status TaskGrpcServiceImpl::DeleteTodo(ServerContext* context, const DeleteTodoReq* req, DeleteTodoRsp* resp) {
    int id = req->id();
    int uid = req->uid();

    LOG_INFO("[Task] DeleteTodo id={} uid={}", id, uid);

    bool ok = MySQLManager::getInstance().deleteTodo(id, uid);
    if (!ok) {
        resp->set_error(static_cast<int>(ErrorCodes::TODO_NOT_FOUND));
        return Status::OK;
    }
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    return Status::OK;
}

Status TaskGrpcServiceImpl::Checkin(ServerContext* context, const CheckinReq* req, CheckinRsp* resp) {
    int uid = req->uid();
    LOG_INFO("[Task] Checkin uid={}", uid);

    int result = MySQLManager::getInstance().checkin(uid);
    if (result == -2) {
        resp->set_error(static_cast<int>(ErrorCodes::CHECKIN_ALREADY_DONE));
        return Status::OK;
    }
    if (result <= 0) {
        resp->set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
        return Status::OK;
    }
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    return Status::OK;
}

Status TaskGrpcServiceImpl::GetCheckins(ServerContext* context, const GetCheckinsReq* req,
                                         GetCheckinsRsp* resp) {
    int uid = req->uid();
    std::string date_from = req->date_from();
    std::string date_to = req->date_to();
    LOG_INFO("[Task] GetCheckins uid={} from={} to={}", uid, date_from, date_to);

    std::vector<CheckinRecord> records;
    bool ok = MySQLManager::getInstance().getCheckins(uid, date_from, date_to, records);
    if (!ok) {
        resp->set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
        return Status::OK;
    }
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    for (auto& r : records) {
        auto* rec = resp->add_records();
        rec->set_uid(r.uid);
        rec->set_checkin_date(r.checkin_date);
        rec->set_created_at(r.created_at);
    }
    return Status::OK;
}
