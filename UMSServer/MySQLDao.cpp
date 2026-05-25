#include "MySQLDao.h"
#include "ConfigManager.h"
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

int MySQLDao::registerUser(const std::string& username, const std::string& email, const std::string& password) {
    // connectionGuard中取出的连接会在生命周期结束时自动返还连接
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        std::cerr << "registering user : " << username << std::endl;
        auto& sql_conn = connection.get()->getConn();
        // 调用存储过程
        // prepareStatement表示调用的语句带参数
        // 用户输入的参数统一使用preparestatement防止SQL注入
        std::unique_ptr<sql::PreparedStatement> stmt(sql_conn->prepareStatement("CALL reg_user(?,?,?,@result)"));
        stmt->setString(1, username);
        stmt->setString(2, email);
        stmt->setString(3, password);
        stmt->execute();

        std::unique_ptr<sql::Statement> stmtResult(sql_conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
        if (!res or !res->next()) return -1;

        int result = res->getInt("result");
        std::cerr << "reg_user result code: " << result << std::endl;
        if (result != 0) return -1;  // 1=用户名重复 2=邮箱重复 -1=异常

        // Registration succeeded — fetch the new uid
        std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement("SELECT uid FROM user WHERE email = ? LIMIT 1"));
        pstmt->setString(1, email);
        std::unique_ptr<sql::ResultSet> uidRes(pstmt->executeQuery());
        if (uidRes and uidRes->next()) {
            int uid = uidRes->getInt("uid");
            std::cerr << "new user uid: " << uid << std::endl;
            return uid;
        }
        return -1;
    } catch(const sql::SQLException& exp) {
        std::cerr << "SQLException: " << exp.what();
        std::cerr << " (MySQL error code: " << exp.getErrorCode();
        std::cerr << ", SQLState: " << exp.getSQLState() << " )" << std::endl;
        return -1;
    }
}

bool MySQLDao::userResetpass(const std::string& username, const std::string& email, const std::string& new_password) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        std::cerr << "Resetting password for user: " << username << std::endl;
        auto& sql_conn = connection.get()->getConn();
        // 先查询用户名和邮箱是否匹配存在
        std::string check_sql = "SELECT 1 FROM user WHERE username = ? AND email = ? LIMIT 1";
        std::unique_ptr<sql::PreparedStatement> pstmt_check(sql_conn->prepareStatement(check_sql));
        pstmt_check->setString(1, username);
        pstmt_check->setString(2, email);
        std::unique_ptr<sql::ResultSet> res(pstmt_check->executeQuery());
        // 没有匹配到记录返回 false
        // 分别对应没有查找到结果集或者结果集为空
        if(!res or !res->next()) {
            std::cerr << "Error: Username and email do not match or not exist." << std::endl;
            return false;
        }
        // 匹配成功 → 更新密码
        std::string update_sql = "UPDATE user SET password = ? WHERE username = ? AND email = ?";
        std::unique_ptr<sql::PreparedStatement> pstmt_update(sql_conn->prepareStatement(update_sql));
        pstmt_update->setString(1, new_password);
        pstmt_update->setString(2, username);
        pstmt_update->setString(3, email);
        pstmt_update->executeUpdate();
        std::cerr << "Success: Password reset for user: " << username << std::endl;
        return true;
    } 
    catch (const sql::SQLException& exp) {
        std::cerr << "SQLException: " << exp.what();
        std::cerr << " (MySQL error code: " << exp.getErrorCode();
        std::cerr << ", SQLState: " << exp.getSQLState() << " )" << std::endl;
        return false;
    }
}

bool MySQLDao::checkLogin(const std::string& email, const std::string& password, UserInfo& userinfo) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        std::cerr << "User: " << email << " is logging in!" << std::endl;
        auto& sql_conn = connection.get()->getConn();
        std::string query = "SELECT uid, username, role, belong_captain_id FROM user WHERE email = ? AND password = ? LIMIT 1";
        std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(query));
        pstmt->setString(1, email);
        pstmt->setString(2, password);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if(!res or !res->next()) {
            std::cerr << "Error: password and email do not match or user not exist." << std::endl;
            return false;
        }
        userinfo.email = email;
        userinfo.username = res->getString("username");
        userinfo.password = password;
        userinfo.uid = res->getInt("uid");
        userinfo.role = res->getInt("role");
        userinfo.belong_captain_id = res->getInt("belong_captain_id");
        std::cerr << "user: " << email << " has login!" << std::endl;
        return true;

    } catch(const sql::SQLException& exp) {
        std::cerr << "SQLException: " << exp.what();
        std::cerr << " (MySQL error code: " << exp.getErrorCode();
        std::cerr << ", SQLState: " << exp.getSQLState() << " )" << std::endl;
        return false;
    }
}

bool MySQLDao::updateTeamInfo(int uid, int belong_captain_id) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        std::string update_sql = "UPDATE user SET belong_captain_id = ? WHERE uid = ?";
        std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(update_sql));
        pstmt->setInt(1, belong_captain_id);
        pstmt->setInt(2, uid);
        int affected = pstmt->executeUpdate();
        return affected > 0;
    } catch(const sql::SQLException& exp) {
        std::cerr << "SQLException in updateTeamInfo: " << exp.what() << std::endl;
        return false;
    }
}