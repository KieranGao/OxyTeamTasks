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
    bool updateTeamInfo(int uid, int belong_captain_id);
    bool updateUserTeam(int uid, int belong_team_id);
    bool listPendingUsers(std::vector<PendingUserInfo>& users);
    bool approveUser(int uid, int role, int belong_team_id);
    bool rejectUser(int uid);
    bool setUserRole(int uid, int role, int belong_team_id);
    bool listAllUsers(std::vector<PendingUserInfo>& users);
private:
    std::unique_ptr<MySQLConnectPool> pool_;
};

#endif /* MYSQLDAO_H */
