#include "LogicSystem.h"
#include "HttpConnection.h"
#include "UserGrpcClient.h"
#include "StatusGrpcClient.h"
#include "MySQLManager.h"

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
        std::cerr << "[Gate] GET_VERIFY_CODE: " << body << std::endl;
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
        std::cerr << "[Gate] USER_REGISTER: " << body << std::endl;
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
        std::cerr << "[Gate] USER_RESETPASS: " << body << std::endl;
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
        std::cerr << "[Gate] USER_LOGIN: " << body << std::endl;
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

        // Step 1: Authenticate via UMSServer
        LoginRsp loginRsp = UserGrpcClient::getInstance().login(email, password);
        if(loginRsp.error() != 0) {
            jsonResp["error"] = loginRsp.error();
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }

        // Step 2: Get user info from MySQL (still needed for username/role/team)
        UserInfo userinfo;
        MySQLManager::getInstance().getUserInfo(loginRsp.uid(), userinfo);

        // Step 3: Allocate PushServer via StatusServer
        AllocateRsp pushRsp = StatusGrpcClient::getInstance().allocatePushServer(loginRsp.uid());
        if(pushRsp.error() != 0) {
            std::cerr << "[Gate] AllocatePushServer failed: " << pushRsp.error() << std::endl;
            jsonResp["error"] = static_cast<int>(ErrorCodes::RPC_ERROR);
            beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
            return;
        }

        jsonResp["error"] = 0;
        jsonResp["email"] = userinfo.email;
        jsonResp["username"] = userinfo.username;
        jsonResp["uid"] = userinfo.uid;
        jsonResp["role"] = userinfo.role;
        jsonResp["belong_captain_id"] = userinfo.belong_captain_id;
        jsonResp["belong_team_id"] = userinfo.belong_team_id;
        jsonResp["token"] = pushRsp.token();
        jsonResp["host"] = pushRsp.host();
        std::cerr << "[Gate] Login OK: " << userinfo.username << " uid=" << userinfo.uid << " role=" << userinfo.role << std::endl;
        beast::ostream(connection->resp_.body()) << jsonResp.toStyledString();
    });

    // ---- Team management (direct MySQL — to be moved to UMSServer later) ----

    registerPost("/user_update_team", [](std::shared_ptr<HttpConnection> connection) {
        auto body = beast::buffers_to_string(connection->req_.body().data());
        std::cerr << "[Gate] UPDATE_TEAM: " << body << std::endl;
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
