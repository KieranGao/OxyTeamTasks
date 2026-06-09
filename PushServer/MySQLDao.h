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

    struct MessageRow {
        int64_t id;
        std::string type;
        std::string title;
        std::string content;
        int is_read;
        std::string created_at;
    };
    // 分页查询消息（MySQL 主数据源，page>1 时使用）
    bool getMessages(int uid, int offset, int limit, std::vector<MessageRow>& messages);
    // 查询未读消息数（登录时校准 Redis unread 计数）
    long getUnreadCount(int uid);
private:
    std::unique_ptr<MySQLConnectPool> pool_;
};

#endif /* MYSQLDAO_H */
