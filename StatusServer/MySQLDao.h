#ifndef MYSQLDAO_H
#define MYSQLDAO_H

#include "MySQLConnectPool.h"

class MySQLDao {
public:
    MySQLDao();
    ~MySQLDao();
    bool userResetpass(const std::string& username, const std::string& email, const std::string& new_password);
    int registerUser(const std::string& username, const std::string& email, const std::string& password);
    bool checkLogin(const std::string& email, const std::string& password, UserInfo& userinfo);
private:
    std::unique_ptr<MySQLConnectPool> pool_;
};

#endif /* MYSQLDAO_H */
