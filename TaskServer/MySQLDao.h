#ifndef MYSQLDAO_H
#define MYSQLDAO_H

#include "MySQLConnectPool.h"
#include "Global.h"
#include <string>
#include <vector>

class MySQLDao {
public:
    MySQLDao();
    ~MySQLDao();

    int createTask(int uid, const std::string& title, const std::string& description,
                   int priority, const std::string& deadline, const std::string& assigned_to);
    bool updateTask(int id, int uid, const std::string& title, const std::string& description,
                    int status, int priority, const std::string& deadline, const std::string& assigned_to);
    bool deleteTask(int id, int uid);
    bool getTask(int id, TaskInfo& info);
    bool listTasks(int uid, int status, const std::string& assigned_to, std::vector<TaskInfo>& tasks);

    int addTodo(int uid, const std::string& content, int priority, const std::string& deadline);
    bool listTodo(int uid, int is_finished, std::vector<TodoInfo>& todos);
    bool updateTodo(int id, int uid, const std::string& content, int priority,
                    const std::string& deadline, int is_finished);
    bool deleteTodo(int id, int uid);
    int getAssigneeStatus(int taskId, int uid);

    struct DeadlineTask { int id; int uid; std::string title; std::string assigned_to; };
    std::vector<DeadlineTask> getDeadlineTasksToday();
    std::vector<int> getUncheckedInUsersToday();

    int checkin(int uid);
    bool getCheckins(int uid, const std::string& date_from, const std::string& date_to,
                     std::vector<CheckinRecord>& records);

private:
    std::unique_ptr<MySQLConnectPool> pool_;
};

#endif /* MYSQLDAO_H */
