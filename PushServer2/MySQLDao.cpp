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

std::vector<int> MySQLDao::getUsersByTeam(int teamId) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    std::vector<int> uids;
    try {
        auto& sql_conn = connection.get()->getConn();
        std::string query = "SELECT uid FROM user WHERE belong_team_id = ? AND status = 1";
        std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(query));
        pstmt->setInt(1, teamId);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res && res->next()) {
            uids.push_back(res->getInt("uid"));
        }
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in getUsersByTeam: {}", exp.what());
    }
    return uids;
}

bool MySQLDao::insertMessage(int uid, const std::string& type, const std::string& title, const std::string& content) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        std::string sql = "INSERT INTO messages (uid, type, title, content) VALUES (?, ?, ?, ?)";
        std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(sql));
        pstmt->setInt(1, uid);
        pstmt->setString(2, type);
        pstmt->setString(3, title);
        pstmt->setString(4, content);
        return pstmt->executeUpdate() > 0;
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in insertMessage: {}", exp.what());
        return false;
    }
}

bool MySQLDao::markMessagesRead(int uid, const std::vector<int64_t>& ids) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        if (ids.empty()) {
            // 全部置为已读
            std::string sql = "UPDATE messages SET is_read = 1 WHERE uid = ? AND is_read = 0";
            std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(sql));
            pstmt->setInt(1, uid);
            pstmt->executeUpdate();
        } else {
            //置为部分id为已读
            for (int64_t id : ids) {
                std::string sql = "UPDATE messages SET is_read = 1 WHERE id = ? AND uid = ?";
                std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(sql));
                pstmt->setInt64(1, id);
                pstmt->setInt(2, uid);
                pstmt->executeUpdate();
            }
        }
        return true;
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in markMessagesRead: {}", exp.what());
        return false;
    }
}

bool MySQLDao::deleteMessages(int uid, const std::vector<int64_t>& ids) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        for (int64_t id : ids) {
            std::string sql = "DELETE FROM messages WHERE id = ? AND uid = ?";
            std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(sql));
            pstmt->setInt64(1, id);
            pstmt->setInt(2, uid);
            pstmt->executeUpdate();
        }
        return true;
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in deleteMessages: {}", exp.what());
        return false;
    }
}

bool MySQLDao::getMessages(int uid, int offset, int limit, std::vector<MessageRow>& messages) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        std::string sql = "SELECT id, type, title, content, is_read, "
                          "DATE_FORMAT(created_at,'%Y-%m-%d %H:%i:%s') AS created_at "
                          "FROM messages WHERE uid = ? ORDER BY created_at DESC LIMIT ? OFFSET ?";
        std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(sql));
        pstmt->setInt(1, uid);
        pstmt->setInt(2, limit);
        pstmt->setInt(3, offset);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res && res->next()) {
            MessageRow row;
            row.id = res->getInt64("id");
            row.type = res->getString("type");
            row.title = res->getString("title");
            row.content = res->isNull("content") ? "" : res->getString("content");
            row.is_read = res->getInt("is_read");
            row.created_at = res->getString("created_at");
            messages.push_back(row);
        }
        return true;
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in getMessages: {}", exp.what());
        return false;
    }
}

long MySQLDao::getUnreadCount(int uid) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        std::string sql = "SELECT COUNT(*) AS cnt FROM messages WHERE uid = ? AND is_read = 0";
        std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(sql));
        pstmt->setInt(1, uid);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res && res->next()) {
            return res->getInt64("cnt");
        }
        return 0;
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in getUnreadCount: {}", exp.what());
        return 0;
    }
}