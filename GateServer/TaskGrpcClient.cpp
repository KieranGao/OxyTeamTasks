#include "TaskGrpcClient.h"
#include "Logger.h"

TaskGrpcClient::TaskGrpcClient() {
    auto& g_config = ConfigManager::getInstance();
    std::string host = g_config["TaskServer"]["host"];
    std::string port = g_config["TaskServer"]["port"];
    rpc_pool_ = std::make_unique<TaskConnectPool>(8, host, port);
}

CreateTaskRsp TaskGrpcClient::createTask(int uid, const std::string& title, const std::string& description,
                                         int priority, const std::string& deadline, const std::string& assigned_to) {
    CreateTaskReq request;
    CreateTaskRsp response;
    ClientContext context;
    request.set_uid(uid);
    request.set_title(title);
    request.set_description(description);
    request.set_priority(priority);
    request.set_deadline(deadline);
    request.set_assigned_to(assigned_to);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->CreateTask(&context, request, &response);
    if (!status.ok()) {
        LOG_ERROR("TaskService CreateTask RPC failed: {}", status.error_message());
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

UpdateTaskRsp TaskGrpcClient::updateTask(int id, int uid, const std::string& title, const std::string& description,
                                         int statusVal, int priority, const std::string& deadline, const std::string& assigned_to) {
    UpdateTaskReq request;
    UpdateTaskRsp response;
    ClientContext context;
    request.set_id(id);
    request.set_uid(uid);
    request.set_title(title);
    request.set_description(description);
    request.set_status(statusVal);
    request.set_priority(priority);
    request.set_deadline(deadline);
    request.set_assigned_to(assigned_to);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->UpdateTask(&context, request, &response);
    if (!status.ok()) {
        LOG_ERROR("TaskService UpdateTask RPC failed: {}", status.error_message());
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

DeleteTaskRsp TaskGrpcClient::deleteTask(int id, int uid) {
    DeleteTaskReq request;
    DeleteTaskRsp response;
    ClientContext context;
    request.set_id(id);
    request.set_uid(uid);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->DeleteTask(&context, request, &response);
    if (!status.ok()) {
        LOG_ERROR("TaskService DeleteTask RPC failed: {}", status.error_message());
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

GetTaskRsp TaskGrpcClient::getTask(int id) {
    GetTaskReq request;
    GetTaskRsp response;
    ClientContext context;
    request.set_id(id);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status status = stub->GetTask(&context, request, &response);
    if (!status.ok()) {
        LOG_ERROR("TaskService GetTask RPC failed: {}", status.error_message());
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

ListTasksRsp TaskGrpcClient::listTasks(int uid, int status, const std::string& assigned_to) {
    ListTasksReq request;
    ListTasksRsp response;
    ClientContext context;
    request.set_uid(uid);
    request.set_status(status);
    request.set_assigned_to(assigned_to);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status s = stub->ListTasks(&context, request, &response);
    if (!s.ok()) {
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

AddTodoRsp TaskGrpcClient::addTodo(int uid, const std::string& content, int priority, const std::string& deadline) {
    AddTodoReq request;
    AddTodoRsp response;
    ClientContext context;
    request.set_uid(uid);
    request.set_content(content);
    request.set_priority(priority);
    request.set_deadline(deadline);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status s = stub->AddTodo(&context, request, &response);
    if (!s.ok()) {
        LOG_ERROR("TaskService AddTodo RPC failed: {}", s.error_message());
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

ListTodoRsp TaskGrpcClient::listTodo(int uid, int is_finished) {
    ListTodoReq request;
    ListTodoRsp response;
    ClientContext context;
    request.set_uid(uid);
    request.set_is_finished(is_finished);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status s = stub->ListTodo(&context, request, &response);
    if (!s.ok()) {
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

DeleteTodoRsp TaskGrpcClient::deleteTodo(int id, int uid) {
    DeleteTodoReq request;
    DeleteTodoRsp response;
    ClientContext context;
    request.set_id(id);
    request.set_uid(uid);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status s = stub->DeleteTodo(&context, request, &response);
    if (!s.ok()) {
        LOG_ERROR("TaskService DeleteTodo RPC failed: {}", s.error_message());
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

UpdateTodoRsp TaskGrpcClient::updateTodo(int id, int uid, const std::string& content, int priority,
                                         const std::string& deadline, int is_finished) {
    UpdateTodoReq request;
    UpdateTodoRsp response;
    ClientContext context;
    request.set_id(id);
    request.set_uid(uid);
    request.set_content(content);
    request.set_priority(priority);
    request.set_deadline(deadline);
    request.set_is_finished(is_finished);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status s = stub->UpdateTodo(&context, request, &response);
    if (!s.ok()) {
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

CheckinRsp TaskGrpcClient::checkin(int uid) {
    CheckinReq request;
    CheckinRsp response;
    ClientContext context;
    request.set_uid(uid);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status s = stub->Checkin(&context, request, &response);
    if (!s.ok()) {
        LOG_ERROR("TaskService Checkin RPC failed: {}", s.error_message());
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}

GetCheckinsRsp TaskGrpcClient::getCheckins(int uid, const std::string& date_from,
                                            const std::string& date_to) {
    GetCheckinsReq request;
    GetCheckinsRsp response;
    ClientContext context;
    request.set_uid(uid);
    request.set_date_from(date_from);
    request.set_date_to(date_to);
    auto stub = rpc_pool_->getStub();
    Defer defer([&stub, this](){ rpc_pool_->returnStub(std::move(stub)); });
    Status s = stub->GetCheckins(&context, request, &response);
    if (!s.ok()) {
        response.set_error(static_cast<int32_t>(ErrorCodes::RPC_ERROR));
    }
    return response;
}
