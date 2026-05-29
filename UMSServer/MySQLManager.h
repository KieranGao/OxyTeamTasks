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
    bool updateTeamInfo(int uid, int belong_captain_id);
    bool updateUserTeam(int uid, int belong_team_id);
    bool listPendingUsers(std::vector<PendingUserInfo>& users);
    bool approveUser(int uid, int role, int belong_team_id);
    bool rejectUser(int uid);
    bool setUserRole(int uid, int role, int belong_team_id);
    bool listAllUsers(std::vector<PendingUserInfo>& users);
private:
    MySQLManager();
    std::unique_ptr<MySQLDao> dao_;
};


#endif /* MYSQLMANAGER_H */
