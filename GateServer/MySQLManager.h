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
    bool updateTeamInfo(int uid, int belong_team_id);
    bool getUserInfo(int uid, UserInfo& userinfo);
    std::vector<int> getUsersByRole(int role);

    bool listMessages(int uid, int page, int pageSize, std::vector<MySQLDao::MessageRow>& messages, int& total);
    bool markMessagesRead(int uid, const std::vector<int64_t>& ids);
    bool deleteMessages(int uid, const std::vector<int64_t>& ids);
    bool insertMessage(int uid, const std::string& type, const std::string& title, const std::string& content);
private:
    MySQLManager();
    std::unique_ptr<MySQLDao> dao_;
};


#endif /* MYSQLMANAGER_H */
