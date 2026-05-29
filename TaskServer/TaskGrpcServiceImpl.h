#ifndef TASKGRPCSERVICEIMPL_H
#define TASKGRPCSERVICEIMPL_H

#include "grpcpp/grpcpp.h"
#include "message.grpc.pb.h"
#include "Global.h"

using grpc::Server;
using grpc::Status;
using grpc::ServerContext;
using grpc::ServerBuilder;

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

class TaskGrpcServiceImpl final : public TaskService::Service {
public:
    TaskGrpcServiceImpl();
    Status CreateTask(ServerContext* context, const CreateTaskReq* req, CreateTaskRsp* resp) override;
    Status UpdateTask(ServerContext* context, const UpdateTaskReq* req, UpdateTaskRsp* resp) override;
    Status DeleteTask(ServerContext* context, const DeleteTaskReq* req, DeleteTaskRsp* resp) override;
    Status GetTask(ServerContext* context, const GetTaskReq* req, GetTaskRsp* resp) override;
    Status ListTasks(ServerContext* context, const ListTasksReq* req, ListTasksRsp* resp) override;
    Status AddTodo(ServerContext* context, const AddTodoReq* req, AddTodoRsp* resp) override;
    Status ListTodo(ServerContext* context, const ListTodoReq* req, ListTodoRsp* resp) override;
    Status UpdateTodo(ServerContext* context, const UpdateTodoReq* req, UpdateTodoRsp* resp) override;
    Status DeleteTodo(ServerContext* context, const DeleteTodoReq* req, DeleteTodoRsp* resp) override;
    Status Checkin(ServerContext* context, const CheckinReq* req, CheckinRsp* resp) override;
    Status GetCheckins(ServerContext* context, const GetCheckinsReq* req, GetCheckinsRsp* resp) override;
};

#endif /* TASKGRPCSERVICEIMPL_H */
