#include "MySQLDao.h"
#include "ConfigManager.h"
#include "Global.h"
#include "Logger.h"
MySQLDao::MySQLDao() {
    ConfigManager& config = ConfigManager::getInstance();
    std::string host = config["MySQL"]["host"];
    std::string port = config["MySQL"]["port"];
    std::string user = config["MySQL"]["user"];
    std::string dbName = config["MySQL"]["dbName"];
    std::string password = config["MySQL"]["password"];
    pool_ = std::make_unique<MySQLConnectPool>(5, host + ":" + port, user, password, dbName);
}

MySQLDao::~MySQLDao() {
    pool_->stop();
}

bool MySQLDao::checkLogin(const std::string& email, const std::string& password, UserInfo& userinfo) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        LOG_DEBUG("User: {} is logging in!", email);
        auto& sql_conn = connection.get()->getConn();
        std::string query = "SELECT uid, username, role, belong_captain_id, belong_team_id FROM user WHERE email = ? AND password = ? LIMIT 1";
        std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(query));
        pstmt->setString(1, email);
        pstmt->setString(2, password);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if(!res or !res->next()) {
            LOG_ERROR("Error: password and email do not match or user not exist.");
            return false;
        }
        userinfo.email = email;
        userinfo.username = res->getString("username");
        userinfo.password = password;
        userinfo.uid = res->getInt("uid");
        userinfo.role = res->getInt("role");
        userinfo.belong_captain_id = res->isNull("belong_captain_id") ? 0 : res->getInt("belong_captain_id");
        userinfo.belong_team_id = res->isNull("belong_team_id") ? 0 : res->getInt("belong_team_id");
        LOG_DEBUG("user: {} has login!", email);
        return true;

    } catch(const sql::SQLException& exp) {
        LOG_ERROR("SQLException: {}", exp.what());
        LOG_ERROR(" (MySQL error code: {}", exp.getErrorCode());
        LOG_DEBUG(", SQLState: {} )", exp.getSQLState());
        return false;
    }
}

std::shared_ptr<UserInfo> MySQLDao::getUser(int uid) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        std::string query = "SELECT username, email, role, belong_captain_id, belong_team_id FROM user WHERE uid = ? LIMIT 1";
        std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(query));
        pstmt->setInt(1, uid);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        std::shared_ptr<UserInfo> userinfo = nullptr;
        if(res and res->next()) {
            userinfo = std::make_shared<UserInfo>();
            userinfo->uid = uid;
            userinfo->username = res->getString("username");
            userinfo->email = res->getString("email");
            userinfo->role = res->getInt("role");
            userinfo->belong_captain_id = res->isNull("belong_captain_id") ? 0 : res->getInt("belong_captain_id");
            userinfo->belong_team_id = res->isNull("belong_team_id") ? 0 : res->getInt("belong_team_id");
        }
        return userinfo;
    }
    catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException: {}", exp.what());
        LOG_ERROR(" (MySQL error code: {}", exp.getErrorCode());
        LOG_DEBUG(", SQLState: {} )", exp.getSQLState());
        return nullptr;
    }
};