#ifndef MYSQLMANAGER_H
#define MYSQLMANAGER_H

#include "MySQLDao.h"
#include "Singleton.h"
#include "Global.h"

class MySQLManager : public Singleton<MySQLManager> {
    friend class Singleton<MySQLManager>;
public:
    ~MySQLManager() = default;
    int registerUser(const std::string& username, const std::string& email, const std::string& password);
    bool userResetpass(const std::string& username, const std::string& email, const std::string& new_password);
    bool checkLogin(const std::string& email, const std::string& password, UserInfo& userinfo);
    std::shared_ptr<UserInfo> getUser(int uid);
    std::vector<int> getUsersByTeam(int teamId);
    bool insertMessage(int uid, const std::string& type, const std::string& title, const std::string& content);
    bool markMessagesRead(int uid, const std::vector<int64_t>& ids);
    bool deleteMessages(int uid, const std::vector<int64_t>& ids);
    bool getMessages(int uid, int offset, int limit, std::vector<MySQLDao::MessageRow>& messages);
    long getUnreadCount(int uid);
private:
    MySQLManager();
    std::unique_ptr<MySQLDao> dao_;
};


#endif /* MYSQLMANAGER_H */
