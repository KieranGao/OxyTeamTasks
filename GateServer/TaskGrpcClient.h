#ifndef TASKGRPCCLIENT_H
#define TASKGRPCCLIENT_H

#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "Global.h"
#include "Singleton.h"
#include "RPCConnectPool.h"
#include "ConfigManager.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using message::CreateTaskReq;
using message::CreateTaskRsp;
using message::UpdateTaskReq;
using message::UpdateTaskRsp;
using message::DeleteTaskReq;
using message::DeleteTaskRsp;
using message::GetTaskReq;
using message::GetTaskRsp;
using message::ListTasksReq;
using message::ListTasksRsp;
using message::AddTodoReq;
using message::AddTodoRsp;
using message::ListTodoReq;
using message::ListTodoRsp;
using message::UpdateTodoReq;
using message::UpdateTodoRsp;
using message::DeleteTodoReq;
using message::DeleteTodoRsp;
using message::CheckinReq;
using message::CheckinRsp;
using message::GetCheckinsReq;
using message::GetCheckinsRsp;
using message::TaskService;

class TaskGrpcClient : public Singleton<TaskGrpcClient> {
    friend class Singleton<TaskGrpcClient>;
public:
    CreateTaskRsp createTask(int uid, const std::string& title, const std::string& description,
                             int priority, const std::string& deadline, const std::string& assigned_to);
    UpdateTaskRsp updateTask(int id, int uid, const std::string& title, const std::string& description,
                             int status, int priority, const std::string& deadline, const std::string& assigned_to);
    DeleteTaskRsp deleteTask(int id, int uid);
    GetTaskRsp getTask(int id);
    ListTasksRsp listTasks(int uid, int status, const std::string& assigned_to);
    AddTodoRsp addTodo(int uid, const std::string& content, int priority, const std::string& deadline);
    ListTodoRsp listTodo(int uid, int is_finished);
    UpdateTodoRsp updateTodo(int id, int uid, const std::string& content, int priority,
                             const std::string& deadline, int is_finished);
    DeleteTodoRsp deleteTodo(int id, int uid);
    CheckinRsp checkin(int uid);
    GetCheckinsRsp getCheckins(int uid, const std::string& date_from, const std::string& date_to);

private:
    std::unique_ptr<TaskConnectPool> rpc_pool_;
    TaskGrpcClient();
};

#endif /* TASKGRPCCLIENT_H */
