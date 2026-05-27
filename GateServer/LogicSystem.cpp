#include "LogicSystem.h"
#include "AsyncTaskPool.h"
#include "HttpConnection.h"
#include "UserGrpcClient.h"
#include "StatusGrpcClient.h"
#include "MySQLManager.h"
#include "Logger.h"

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

    // ---- UserService endpoints (via gRPC to UMSServer) ----

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
                // Step 1: Authenticate via UMSServer
                LoginRsp loginRsp = UserGrpcClient::getInstance().login(email, password);
                if(loginRsp.error() != 0) {
                    result["error"] = loginRsp.error();
                    p->set_value(result);
                    return;
                }

                // Step 2: Get user info from MySQL
                UserInfo userinfo;
                MySQLManager::getInstance().getUserInfo(loginRsp.uid(), userinfo);

                // Step 3: Allocate PushServer via StatusServer
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

    // ---- Team management (direct MySQL — to be moved to UMSServer later) ----

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
        bool ret = MySQLManager::getInstance().updateTeamInfo(uid, belong_team_id);
        jsonResp["error"] = ret ? 0 : static_cast<int>(ErrorCodes::USER_ID_INVALID);
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
