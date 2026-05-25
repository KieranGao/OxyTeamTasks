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

bool MySQLDao::checkLogin(const std::string& email, const std::string& password, UserInfo& userinfo) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        std::cerr << "User: " << email << " is logging in!" << std::endl;
        auto& sql_conn = connection.get()->getConn();
        std::string query = "SELECT uid, username FROM user WHERE email = ? AND password = ? LIMIT 1";
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
        std::cerr << "user: " << email << " has login!" << std::endl;
        return true;

    } catch(const sql::SQLException& exp) {
        std::cerr << "SQLException: " << exp.what();
        std::cerr << " (MySQL error code: " << exp.getErrorCode();
        std::cerr << ", SQLState: " << exp.getSQLState() << " )" << std::endl;
        return false;
    }
}