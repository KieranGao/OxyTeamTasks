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
    bool updateTeamInfo(int uid, int belong_team_id);
    bool getUserInfo(int uid, UserInfo& userinfo);
    std::vector<int> getUsersByRole(int role);

    struct MessageRow { int64_t id; std::string type, title, content, created_at; int is_read; };
    bool listMessages(int uid, int page, int pageSize, std::vector<MessageRow>& messages, int& total);
    bool markMessagesRead(int uid, const std::vector<int64_t>& ids);
    bool deleteMessages(int uid, const std::vector<int64_t>& ids);
    bool insertMessage(int uid, const std::string& type, const std::string& title, const std::string& content);
private:
    std::unique_ptr<MySQLConnectPool> pool_;
};

#endif /* MYSQLDAO_H */
