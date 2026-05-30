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
    std::shared_ptr<UserInfo> getUser(int uid);
    std::vector<int> getUsersByTeam(int teamId);
    bool insertMessage(int uid, const std::string& type, const std::string& title, const std::string& content);
    bool markMessagesRead(int uid, const std::vector<int64_t>& ids);
    bool deleteMessages(int uid, const std::vector<int64_t>& ids);
private:
    std::unique_ptr<MySQLConnectPool> pool_;
};

#endif /* MYSQLDAO_H */
