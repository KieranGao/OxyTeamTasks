#include "LogicSystem.h"
#include "AsyncTaskPool.h"
#include "HttpConnection.h"
#include "UserGrpcClient.h"
#include "StatusGrpcClient.h"
#include "TaskGrpcClient.h"
#include "MySQLManager.h"
#include "RedisManager.h"
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/random_generator.hpp"
#include "boost/uuid/uuid_io.hpp"
#include "Logger.h"

static std::string generate_lock_owner() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}

void LogicSystem::registerGet(std::string url, HttpHandler handler) {
    getHandlers_[url] = handler;
}

void LogicSystem::registerPost(std::string url, HttpHandler handler) {
    postHandlers_[url] = handler;
}

LogicSystem::LogicSystem() {
    registerGet("/get_test", [](std::shared_ptr<HttpConnection> connection) {
        beast::ostream(connection->resp_.body()) << "This is a GET response\r\n";
        int cnt = 0;
        for(auto &[key, value] : connection->get_params_) {
            cnt++;
            beast::ostream(connection->resp_.body()) << "param " << cnt << ": " << key << " = " << value << "\r\n";
        }
    });

    registerPost("/get_verify_code", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        LOG_DEBUG("[Gate] GET_VERIFY_CODE: {}", body);
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if(!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        std::string email = jsonData["email"].asString();
        VerifyRsp rsp = UserGrpcClient::getInstance().getVerifyCode(email);
        jsonResp["error"] = rsp.error();
        jsonResp["email"] = email;
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/user_register", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        LOG_DEBUG("[Gate] USER_REGISTER: {}", body);
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if(!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        RegisterRsp rsp = UserGrpcClient::getInstance().registerUser(
            jsonData["user"].asString(),
            jsonData["email"].asString(),
            jsonData["password"].asString(),
            jsonData["verify_code"].asString()
        );
        jsonResp["error"] = rsp.error();
        if(rsp.error() == 0) {
            jsonResp["user"] = jsonData["user"].asString();
            jsonResp["email"] = jsonData["email"].asString();

            // 通知教练审核新注册用户
            AsyncTaskPool::getInstance().post([username = jsonData["user"].asString(), email = jsonData["email"].asString()]() {
                try {
                    auto coachUids = MySQLManager::getInstance().getUsersByRole(2);
                    std::string title = "新用户注册待审核: " + username;
                    std::string content = "{\"username\":\"" + username + "\",\"email\":\"" + email + "\"}";
                    for (int coachUid : coachUids) {
                        MySQLManager::getInstance().insertMessage(coachUid, "user_register", title, content);
                        // 未读消息+1
                        RedisManager::getInstance().incr("unread:" + std::to_string(coachUid));
                    }
                    LOG_INFO("[Gate] Registration notification sent to {} coaches for user {}", coachUids.size(), username);
                } catch (...) {
                    LOG_ERROR("[Gate] Failed to send registration notification to coaches");
                }
            });
        }
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/user_resetpass", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        LOG_DEBUG("[Gate] USER_RESETPASS: {}", body);
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if(!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        ResetPassRsp rsp = UserGrpcClient::getInstance().resetPass(
            jsonData["user"].asString(),
            jsonData["email"].asString(),
            jsonData["password"].asString(),
            jsonData["verify_code"].asString()
        );
        jsonResp["error"] = rsp.error();
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/user_login", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        LOG_DEBUG("[Gate] USER_LOGIN: {}", body);
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if(!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        std::string email = jsonData["email"].asString();
        std::string password = jsonData["password"].asString();

        // 将阻塞的 gRPC + MySQL 调用投递到线程池，IO 线程用 promise/future 等待（带超时）
        auto p = std::make_shared<std::promise<Json::Value>>();
        auto f = p->get_future();
        AsyncTaskPool::getInstance().post([p, email, password]() {
            Json::Value result;
            try {
                LoginRsp loginRsp = UserGrpcClient::getInstance().login(email, password);
                if(loginRsp.error() != 0) {
                    result["error"] = loginRsp.error();
                    p->set_value(result);
                    return;
                }
                UserInfo userinfo;
                MySQLManager::getInstance().getUserInfo(loginRsp.uid(), userinfo);
                
                AllocateRsp pushRsp = StatusGrpcClient::getInstance().allocatePushServer(loginRsp.uid());
                if(pushRsp.error() != 0) {
                    LOG_ERROR("[Gate] AllocatePushServer failed: {}", pushRsp.error());
                    result["error"] = static_cast<int>(ErrorCodes::RPC_ERROR);
                    p->set_value(result);
                    return;
                }
                LOG_DEBUG("[Gate] Allocated PushServer [ip:{}:{}] for {}",
                         pushRsp.host(), pushRsp.port(), userinfo.username);

                result["error"] = 0;
                result["email"] = userinfo.email;
                result["username"] = userinfo.username;
                result["uid"] = userinfo.uid;
                result["role"] = userinfo.role;
                result["belong_captain_id"] = userinfo.belong_captain_id;
                result["belong_team_id"] = userinfo.belong_team_id;
                result["token"] = pushRsp.token();
                result["port"] = pushRsp.port();
                result["host"] = pushRsp.host();
                LOG_DEBUG("[Gate] Login OK: {} uid={} role={}", userinfo.username, userinfo.uid, userinfo.role);
                p->set_value(result);
            } catch (const std::exception& e) {
                result["error"] = static_cast<int>(ErrorCodes::RPC_ERROR);
                try { p->set_value(result); } catch (...) {}
            } catch (...) {
                result["error"] = static_cast<int>(ErrorCodes::RPC_ERROR);
                try { p->set_value(result); } catch (...) {}
            }
        });

        // IO 线程等待结果，最多 5 秒
        auto status = f.wait_for(std::chrono::seconds(5));
        if (status == std::future_status::ready) {
            try {
                Json::Value result = f.get();
                beast::ostream(connection->resp_.body()) << result.toStyledString();
            } catch (...) {
                jsonResp["error"] = static_cast<int>(ErrorCodes::RPC_ERROR);
                beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            }
        } else {
            jsonResp["error"] = static_cast<int>(ErrorCodes::RPC_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            LOG_ERROR("[Gate] /user_login timed out after 5s");
        }
    });

    registerPost("/user_update_team", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        LOG_DEBUG("[Gate] UPDATE_TEAM: {}", body);
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if(!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        int uid = jsonData["uid"].asInt();
        int belong_team_id = jsonData["belong_team_id"].asInt();
        UpdateTeamInfoRsp rsp = UserGrpcClient::getInstance().updateTeamInfo(uid, belong_team_id);
        jsonResp["error"] = rsp.error();
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    
    registerPost("/user_list_pending", [](std::shared_ptr<HttpConnection> connection) {
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonResp;
        ListPendingUsersRsp rsp = UserGrpcClient::getInstance().listPendingUsers();
        jsonResp["error"] = rsp.error();
        Json::Value users(Json::arrayValue);
        for (int i = 0; i < rsp.users_size(); ++i) {
            auto& u = rsp.users(i);
            Json::Value user;
            user["uid"] = u.uid();
            user["username"] = u.username();
            user["email"] = u.email();
            user["role"] = u.role();
            user["belong_team_id"] = u.belong_team_id();
            users.append(user);
        }
        jsonResp["users"] = users;
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/user_approve", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if(!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        int uid = jsonData["uid"].asInt();
        int role = jsonData.get("role", 0).asInt();
        int belong_team_id = jsonData.get("belong_team_id", 0).asInt();
        ApproveUserRsp rsp = UserGrpcClient::getInstance().approveUser(uid, role, belong_team_id);
        jsonResp["error"] = rsp.error();
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/user_reject", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if(!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        int uid = jsonData["uid"].asInt();
        RejectUserRsp rsp = UserGrpcClient::getInstance().rejectUser(uid);
        jsonResp["error"] = rsp.error();
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/user_set_role", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if(!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        int uid = jsonData["uid"].asInt();
        int role = jsonData.get("role", 0).asInt();
        int belong_team_id = jsonData.get("belong_team_id", 0).asInt();
        SetUserRoleRsp rsp = UserGrpcClient::getInstance().setUserRole(uid, role, belong_team_id);
        jsonResp["error"] = rsp.error();
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/user_list_all", [](std::shared_ptr<HttpConnection> connection) {
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonResp;
        ListAllUsersRsp rsp = UserGrpcClient::getInstance().listAllUsers();
        jsonResp["error"] = rsp.error();
        Json::Value users(Json::arrayValue);
        for (int i = 0; i < rsp.users_size(); ++i) {
            auto& u = rsp.users(i);
            Json::Value user;
            user["uid"] = u.uid();
            user["username"] = u.username();
            user["email"] = u.email();
            user["role"] = u.role();
            user["belong_team_id"] = u.belong_team_id();
            user["status"] = u.status();
            users.append(user);
        }
        jsonResp["users"] = users;
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/monitor/query_logs", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        std::string service, level;
        int limit = 100;
        if (reader.parse(body, jsonData)) {
            service = jsonData.get("service", "").asString();
            level = jsonData.get("level", "").asString();
            limit = jsonData.get("limit", 100).asInt();
        }
        QueryLogsRsp rsp = StatusGrpcClient::getInstance().queryLogs(service, level, limit);
        jsonResp["error"] = rsp.error();
        Json::Value entries(Json::arrayValue);
        for (int i = 0; i < rsp.entries_size(); ++i) {
            auto& e = rsp.entries(i);
            Json::Value entry;
            entry["service"] = e.service();
            entry["level"] = e.level();
            entry["message"] = e.message();
            entry["timestamp"] = (Json::Int64)e.timestamp();
            entries.append(entry);
        }
        jsonResp["entries"] = entries;
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/monitor/server_status", [](std::shared_ptr<HttpConnection> connection) {
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonResp;
        QueryServerStatusRsp rsp = StatusGrpcClient::getInstance().queryServerStatus();
        jsonResp["error"] = rsp.error();
        Json::Value servers(Json::arrayValue);
        for (int i = 0; i < rsp.servers_size(); ++i) {
            auto& s = rsp.servers(i);
            Json::Value srv;
            srv["service"] = s.service();
            srv["host"] = s.host();
            srv["port"] = s.port();
            srv["status"] = s.status();
            srv["last_heartbeat"] = (Json::Int64)s.last_heartbeat();
            srv["connections"] = s.connections();
            servers.append(srv);
        }
        jsonResp["servers"] = servers;
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/task_create", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if (!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        CreateTaskRsp rsp = TaskGrpcClient::getInstance().createTask(
            jsonData["uid"].asInt(),
            jsonData["title"].asString(),
            jsonData.get("description", "").asString(),
            jsonData.get("priority", 3).asInt(),
            jsonData.get("deadline", "").asString(),
            jsonData.get("assigned_to", "0").asString()
        );
        jsonResp["error"] = rsp.error();
        jsonResp["id"] = rsp.id();
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/task_update", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if (!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        UpdateTaskRsp rsp = TaskGrpcClient::getInstance().updateTask(
            jsonData["id"].asInt(),
            jsonData["uid"].asInt(),
            jsonData.get("title", "").asString(),
            jsonData.get("description", "").asString(),
            jsonData.get("status", 0).asInt(),
            jsonData.get("priority", 3).asInt(),
            jsonData.get("deadline", "").asString(),
            jsonData.get("assigned_to", "0").asString()
        );
        jsonResp["error"] = rsp.error();
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/task_delete", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if (!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        DeleteTaskRsp rsp = TaskGrpcClient::getInstance().deleteTask(
            jsonData["id"].asInt(),
            0
        );
        jsonResp["error"] = rsp.error();
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/task_get", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if (!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        GetTaskRsp rsp = TaskGrpcClient::getInstance().getTask(jsonData["id"].asInt());
        jsonResp["error"] = rsp.error();
        if (rsp.error() == 0 && rsp.has_task()) {
            auto& t = rsp.task();
            Json::Value task;
            task["id"] = t.id();
            task["uid"] = t.uid();
            task["title"] = t.title();
            task["description"] = t.description();
            task["status"] = t.status();
            task["priority"] = t.priority();
            task["deadline"] = t.deadline();
            task["assigned_to"] = t.assigned_to();
            task["created_at"] = t.created_at();
            task["updated_at"] = t.updated_at();
            task["my_status"] = t.my_status();
            Json::Value astatuses(Json::arrayValue);
            for (int j = 0; j < t.assignee_statuses_size(); ++j) {
                auto& as = t.assignee_statuses(j);
                Json::Value a;
                a["assignee_uid"] = as.assignee_uid();
                a["status"] = as.status();
                astatuses.append(a);
            }
            task["assignee_statuses"] = astatuses;
            jsonResp["task"] = task;
        }
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/task_list", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if (!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        ListTasksRsp rsp = TaskGrpcClient::getInstance().listTasks(
            jsonData.get("uid", 0).asInt(),
            jsonData.get("status", -1).asInt(),
            jsonData.get("assigned_to", "0").asString()
        );
        jsonResp["error"] = rsp.error();
        Json::Value tasks(Json::arrayValue);
        for (int i = 0; i < rsp.tasks_size(); ++i) {
            auto& t = rsp.tasks(i);
            Json::Value task;
            task["id"] = t.id();
            task["uid"] = t.uid();
            task["title"] = t.title();
            task["description"] = t.description();
            task["status"] = t.status();
            task["priority"] = t.priority();
            task["deadline"] = t.deadline();
            task["assigned_to"] = t.assigned_to();
            task["created_at"] = t.created_at();
            task["updated_at"] = t.updated_at();
            task["my_status"] = t.my_status();
            Json::Value astatuses(Json::arrayValue);
            for (int j = 0; j < t.assignee_statuses_size(); ++j) {
                auto& as = t.assignee_statuses(j);
                Json::Value a;
                a["assignee_uid"] = as.assignee_uid();
                a["status"] = as.status();
                astatuses.append(a);
            }
            task["assignee_statuses"] = astatuses;
            tasks.append(task);
        }
        jsonResp["tasks"] = tasks;
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/todo_add", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if (!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        AddTodoRsp rsp = TaskGrpcClient::getInstance().addTodo(
            jsonData["uid"].asInt(),
            jsonData["content"].asString(),
            jsonData.get("priority", 3).asInt(),
            jsonData.get("deadline", "").asString()
        );
        jsonResp["error"] = rsp.error();
        jsonResp["id"] = rsp.id();
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/todo_list", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if (!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        ListTodoRsp rsp = TaskGrpcClient::getInstance().listTodo(
            jsonData["uid"].asInt(),
            jsonData.get("is_finished", 0).asInt()
        );
        jsonResp["error"] = rsp.error();
        Json::Value todos(Json::arrayValue);
        for (int i = 0; i < rsp.todos_size(); ++i) {
            auto& td = rsp.todos(i);
            Json::Value todo;
            todo["id"] = td.id();
            todo["uid"] = td.uid();
            todo["content"] = td.content();
            todo["priority"] = td.priority();
            todo["deadline"] = td.deadline();
            todo["is_finished"] = td.is_finished();
            todos.append(todo);
        }
        jsonResp["todos"] = todos;
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/todo_delete", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if (!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        DeleteTodoRsp rsp = TaskGrpcClient::getInstance().deleteTodo(
            jsonData["id"].asInt(),
            jsonData["uid"].asInt()
        );
        jsonResp["error"] = rsp.error();
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/checkin", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if (!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        CheckinRsp rsp = TaskGrpcClient::getInstance().checkin(
            jsonData["uid"].asInt()
        );
        jsonResp["error"] = rsp.error();
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/checkin_list", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if (!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        GetCheckinsRsp rsp = TaskGrpcClient::getInstance().getCheckins(
            jsonData.get("uid", 0).asInt(),
            jsonData.get("date_from", "").asString(),
            jsonData.get("date_to", "").asString()
        );
        jsonResp["error"] = rsp.error();
        Json::Value records(Json::arrayValue);
        for (int i = 0; i < rsp.records_size(); ++i) {
            auto& r = rsp.records(i);
            Json::Value rec;
            rec["uid"] = r.uid();
            rec["checkin_date"] = r.checkin_date();
            rec["created_at"] = r.created_at();
            records.append(rec);
        }
        jsonResp["records"] = records;
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/msg_list", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if (!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        int uid = jsonData["uid"].asInt();
        int page = jsonData.get("page", 1).asInt();
        int pageSize = jsonData.get("page_size", 20).asInt();

        std::vector<MySQLDao::MessageRow> messages;
        int total = 0;
        MySQLManager::getInstance().listMessages(uid, page, pageSize, messages, total);

        // 获取未读消息
        std::string unread_str;
        int unread_count = 0;
        if (RedisManager::getInstance().get("unread:" + std::to_string(uid), unread_str)) {
            try { unread_count = std::stoi(unread_str); } catch (...) {}
        }

        jsonResp["error"] = 0;
        jsonResp["unread_count"] = unread_count;
        jsonResp["total"] = total;
        Json::Value arr(Json::arrayValue);
        for (auto& m : messages) {
            Json::Value item;
            item["id"] = static_cast<Json::Int64>(m.id);
            item["type"] = m.type;
            item["title"] = m.title;
            item["content"] = m.content;
            item["is_read"] = m.is_read;
            item["created_at"] = m.created_at;
            arr.append(item);
        }
        jsonResp["messages"] = arr;
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/msg_read", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if (!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        int uid = jsonData["uid"].asInt();
        std::vector<int64_t> ids;
        if (jsonData.isMember("ids")) {
            for (auto& v : jsonData["ids"]) ids.push_back(v.asInt64());
        }
        MySQLManager::getInstance().markMessagesRead(uid, ids);
        // 原子标记已读，分布式锁保护
        std::string uid_str = std::to_string(uid);
        std::string lock_key = "lock:unread:" + uid_str;
        std::string owner = generate_lock_owner();
        if (RedisManager::getInstance().acquireLockWithRetry(lock_key, owner, 10)) {
            int decrement_count = ids.empty() ? 0 : static_cast<int>(ids.size());
            RedisManager::getInstance().markReadAtomic(uid_str, decrement_count);
            RedisManager::getInstance().releaseLock(lock_key, owner);
        } else {
            LOG_ERROR("[Gate] Failed to acquire unread lock for markRead uid={}", uid);
        }
        jsonResp["error"] = 0;
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/msg_delete", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if (!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        int uid = jsonData["uid"].asInt();
        std::vector<int64_t> ids;
        for (auto& v : jsonData["ids"]) ids.push_back(v.asInt64());
        MySQLManager::getInstance().deleteMessages(uid, ids);
        jsonResp["error"] = 0;
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    registerPost("/todo_update", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        connection->resp_.set(http::field::content_type, "application/json");
        Json::Value jsonData, jsonResp;
        Json::Reader reader;
        if (!reader.parse(body, jsonData)) {
            jsonResp["error"] = static_cast<int>(ErrorCodes::JSON_PARSE_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }
        UpdateTodoRsp rsp = TaskGrpcClient::getInstance().updateTodo(
            jsonData["id"].asInt(),
            jsonData["uid"].asInt(),
            jsonData.get("content", "").asString(),
            jsonData.get("priority", 3).asInt(),
            jsonData.get("deadline", "").asString(),
            jsonData.get("is_finished", 2).asInt()
        );
        jsonResp["error"] = rsp.error();
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });
}

bool LogicSystem::handleGet(std::string url, std::shared_ptr<HttpConnection> connection) {
    if(getHandlers_.find(url) == getHandlers_.end()) return false;
    getHandlers_[url](connection);
    return true;
}

bool LogicSystem::handlePost(std::string url, std::shared_ptr<HttpConnection> connection) {
    if(postHandlers_.find(url) == postHandlers_.end()) return false;
    postHandlers_[url](connection);
    return true;
}
