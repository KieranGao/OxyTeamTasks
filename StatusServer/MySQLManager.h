#ifndef MYSQLMANAGER_H
#define MYSQLMANAGER_H

#include "MySQLDao.h"
#include "Singleton.h"

class MySQLManager : public Singleton<MySQLManager> {
    friend class Singleton<MySQLManager>;
public:
    ~MySQLManager() = default;
    int registerUser(const std::string& username, const std::string& email, const std::string& password);
    bool userResetpass(const std::string& username, const std::string& email, const std::string& new_password);
    bool checkLogin(const std::string& email, const std::string& password, UserInfo& userinfo);
private:
    MySQLManager();
    std::unique_ptr<MySQLDao> dao_;
};


#endif /* MYSQLMANAGER_H */
