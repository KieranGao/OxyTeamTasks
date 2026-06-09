#include "MySQLDao.h"
#include "ConfigManager.h"
#include "Logger.h"

MySQLDao::MySQLDao() {
    ConfigManager& config = ConfigManager::getInstance();
    std::string host = config["MySQL"]["host"];
    std::string port = config["MySQL"]["port"];
    std::string user = config["MySQL"]["user"];
    std::string dbName = config["MySQL"]["dbName"];
    std::string password = config["MySQL"]["password"];
    pool_ = std::make_unique<MySQLConnectPool>(5, host + ":" + port, user, password, dbName);
}

MySQLDao::~MySQLDao() {
    pool_->stop();
}

int MySQLDao::createTask(int uid, const std::string& title, const std::string& description,
                         int priority, const std::string& deadline, const std::string& assigned_to) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        sql_conn->setAutoCommit(false);

        // 1. 插入任务
        std::string sql = "INSERT INTO task (uid, title, description, priority, deadline, assigned_to) VALUES (?, ?, ?, ?, ?, ?)";
        std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(sql));
        pstmt->setInt(1, uid);
        pstmt->setString(2, title);
        pstmt->setString(3, description);
        pstmt->setInt(4, priority);
        if (deadline.empty()) {
            pstmt->setNull(5, sql::DataType::TIMESTAMP);
        } else {
            pstmt->setString(5, deadline);
        }
        pstmt->setString(6, assigned_to);
        pstmt->executeUpdate();

        std::unique_ptr<sql::Statement> stmt(sql_conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID() AS id"));
        int newId = -1;
        if (res && res->next()) {
            newId = res->getInt("id");
        }
        if (newId <= 0) {
            sql_conn->rollback();
            sql_conn->setAutoCommit(true);
            return -1;
        }

        // 2. 插入指派关系（任一失败则整体回滚）
        if (!assigned_to.empty() && assigned_to != "0") {
            std::string remain = assigned_to;
            while (!remain.empty()) {
                size_t pos = remain.find(',');
                std::string uidStr = (pos == std::string::npos) ? remain : remain.substr(0, pos);
                int auid = std::stoi(uidStr);
                if (auid > 0) {
                    std::unique_ptr<sql::PreparedStatement> apstmt(sql_conn->prepareStatement(
                        "INSERT INTO task_assignments (task_id, assignee_uid, status) VALUES (?, ?, 0)"));
                    apstmt->setInt(1, newId);
                    apstmt->setInt(2, auid);
                    apstmt->executeUpdate();
                }
                if (pos == std::string::npos) break;
                remain = remain.substr(pos + 1);
            }
        }

        sql_conn->commit();
        sql_conn->setAutoCommit(true);
        return newId;
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in createTask: {}", exp.what());
        try {
            auto& sql_conn = connection.get()->getConn();
            sql_conn->rollback();
            sql_conn->setAutoCommit(true);
        } catch (...) {}
        return -1;
    }
}

bool MySQLDao::updateTask(int id, int uid, const std::string& title, const std::string& description,
                          int status, int priority, const std::string& deadline, const std::string& assigned_to) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        sql_conn->setAutoCommit(false);

        // 1. 更新任务主表
        std::string sql = "UPDATE task SET title = ?, description = ?, status = ?, priority = ?, deadline = ?, assigned_to = ? WHERE id = ?";
        std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(sql));
        pstmt->setString(1, title);
        pstmt->setString(2, description);
        pstmt->setInt(3, status);
        pstmt->setInt(4, priority);
        if (deadline.empty()) {
            pstmt->setNull(5, sql::DataType::TIMESTAMP);
        } else {
            pstmt->setString(5, deadline);
        }
        pstmt->setString(6, assigned_to);
        pstmt->setInt(7, id);
        bool ok = pstmt->executeUpdate() > 0;

        // 2. 更新指派人独立状态
        if (ok && uid > 0) {
            std::unique_ptr<sql::PreparedStatement> apstmt(sql_conn->prepareStatement(
                "INSERT INTO task_assignments (task_id, assignee_uid, status) VALUES (?, ?, ?) "
                "ON DUPLICATE KEY UPDATE status = VALUES(status)"));
            apstmt->setInt(1, id);
            apstmt->setInt(2, uid);
            apstmt->setInt(3, status);
            apstmt->executeUpdate();
        }

        sql_conn->commit();
        sql_conn->setAutoCommit(true);
        return ok;
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in updateTask: {}", exp.what());
        try {
            auto& sql_conn = connection.get()->getConn();
            sql_conn->rollback();
            sql_conn->setAutoCommit(true);
        } catch (...) {}
        return false;
    }
}

bool MySQLDao::deleteTask(int id, int uid) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        sql_conn->setAutoCommit(false);

        // 先删指派关系，再删任务（无外键级联，手动保证一致性）
        {
            std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(
                "DELETE FROM task_assignments WHERE task_id = ?"));
            pstmt->setInt(1, id);
            pstmt->executeUpdate();
        }
        {
            std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(
                "DELETE FROM task WHERE id = ?"));
            pstmt->setInt(1, id);
            pstmt->executeUpdate();
        }

        sql_conn->commit();
        sql_conn->setAutoCommit(true);
        return true;
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in deleteTask: {}", exp.what());
        try {
            auto& sql_conn = connection.get()->getConn();
            sql_conn->rollback();
            sql_conn->setAutoCommit(true);
        } catch (...) {}
        return false;
    }
}

bool MySQLDao::getTask(int id, TaskInfo& info) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(
            "SELECT id, uid, title, COALESCE(description,'') AS description, status, priority, "
            "COALESCE(DATE_FORMAT(deadline,'%Y-%m-%d %H:%i:%s'),'') AS deadline, assigned_to, "
            "DATE_FORMAT(created_at,'%Y-%m-%d %H:%i:%s') AS created_at, "
            "DATE_FORMAT(updated_at,'%Y-%m-%d %H:%i:%s') AS updated_at "
            "FROM task WHERE id = ?"));
        pstmt->setInt(1, id);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (!res || !res->next()) {
            return false;
        }
        info.id = res->getInt("id");
        info.uid = res->getInt("uid");
        info.title = res->getString("title");
        info.description = res->getString("description");
        info.status = res->getInt("status");
        info.priority = res->getInt("priority");
        info.deadline = res->getString("deadline");
        info.assigned_to = res->getString("assigned_to");
        info.created_at = res->getString("created_at");
        info.updated_at = res->getString("updated_at");
        info.my_status = info.status;

        // Fetch assignee_statuses
        std::unique_ptr<sql::PreparedStatement> apstmt(sql_conn->prepareStatement(
            "SELECT assignee_uid, status FROM task_assignments WHERE task_id = ?"));
        apstmt->setInt(1, id);
        std::unique_ptr<sql::ResultSet> ares(apstmt->executeQuery());
        while (ares && ares->next()) {
            AssigneeStatus as;
            as.assignee_uid = ares->getInt("assignee_uid");
            as.status = ares->getInt("status");
            info.assignee_statuses.push_back(as);
        }
        return true;
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in getTask: {}", exp.what());
        return false;
    }
}

bool MySQLDao::listTasks(int uid, int status, const std::string& assigned_to, std::vector<TaskInfo>& tasks) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        std::string sql = "SELECT t.id, t.uid, t.title, COALESCE(t.description,'') AS description, "
                          "t.status, t.priority, "
                          "COALESCE(DATE_FORMAT(t.deadline,'%Y-%m-%d %H:%i:%s'),'') AS deadline, "
                          "t.assigned_to, "
                          "DATE_FORMAT(t.created_at,'%Y-%m-%d %H:%i:%s') AS created_at, "
                          "DATE_FORMAT(t.updated_at,'%Y-%m-%d %H:%i:%s') AS updated_at";
        if (uid > 0) {
            sql += ", COALESCE(ta.status, t.status) AS my_status";
        } else {
            sql += ", t.status AS my_status";
        }
        sql += " FROM task t";
        if (uid > 0) {
            sql += " LEFT JOIN task_assignments ta ON t.id = ta.task_id AND ta.assignee_uid = " + std::to_string(uid);
        }
        sql += " WHERE 1=1";
        if (uid > 0) {
            sql += " AND (t.uid = " + std::to_string(uid) + " OR FIND_IN_SET(" + std::to_string(uid) + ", t.assigned_to) > 0)";
        }
        if (status >= 0) {
            sql += " AND t.status = " + std::to_string(status);
        }
        if (!assigned_to.empty() && assigned_to != "0") {
            sql += " AND FIND_IN_SET(" + assigned_to + ", t.assigned_to) > 0";
        }
        sql += " ORDER BY t.priority ASC, t.deadline ASC";

        std::unique_ptr<sql::Statement> stmt(sql_conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(sql));
        std::vector<int> taskIds;
        while (res && res->next()) {
            TaskInfo info;
            info.id = res->getInt("id");
            info.uid = res->getInt("uid");
            info.title = res->getString("title");
            info.description = res->getString("description");
            info.status = res->getInt("status");
            info.priority = res->getInt("priority");
            info.deadline = res->getString("deadline");
            info.assigned_to = res->getString("assigned_to");
            info.created_at = res->getString("created_at");
            info.updated_at = res->getString("updated_at");
            info.my_status = res->getInt("my_status");
            tasks.push_back(info);
            taskIds.push_back(info.id);
        }

        // Batch-fetch assignee_statuses for all returned tasks
        if (!taskIds.empty()) {
            std::string idList;
            for (size_t i = 0; i < taskIds.size(); ++i) {
                if (i > 0) idList += ",";
                idList += std::to_string(taskIds[i]);
            }
            std::string asql = "SELECT task_id, assignee_uid, status FROM task_assignments WHERE task_id IN (" + idList + ")";
            std::unique_ptr<sql::Statement> astmt(sql_conn->createStatement());
            std::unique_ptr<sql::ResultSet> ares(astmt->executeQuery(asql));
            while (ares && ares->next()) {
                int tid = ares->getInt("task_id");
                AssigneeStatus as;
                as.assignee_uid = ares->getInt("assignee_uid");
                as.status = ares->getInt("status");
                for (auto& t : tasks) {
                    if (t.id == tid) {
                        t.assignee_statuses.push_back(as);
                        break;
                    }
                }
            }
        }
        return true;
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in listTasks: {}", exp.what());
        return false;
    }
}

int MySQLDao::addTodo(int uid, const std::string& content, int priority, const std::string& deadline) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        std::string sql = "INSERT INTO todo_list (uid, content, priority, deadline) VALUES (?, ?, ?, ?)";
        std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(sql));
        pstmt->setInt(1, uid);
        pstmt->setString(2, content);
        pstmt->setInt(3, priority);
        if (deadline.empty()) {
            pstmt->setNull(4, sql::DataType::TIMESTAMP);
        } else {
            pstmt->setString(4, deadline);
        }
        pstmt->executeUpdate();

        std::unique_ptr<sql::Statement> stmt(sql_conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID() AS id"));
        if (res && res->next()) {
            return res->getInt("id");
        }
        return -1;
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in addTodo: {}", exp.what());
        return -1;
    }
}

bool MySQLDao::listTodo(int uid, int is_finished, std::vector<TodoInfo>& todos) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        std::string sql = "SELECT id, uid, content, priority, "
                          "COALESCE(DATE_FORMAT(deadline,'%Y-%m-%d %H:%i:%s'),'') AS deadline, is_finished "
                          "FROM todo_list WHERE uid = ?";
        if (is_finished > 0) {
            sql += " AND is_finished = " + std::to_string(is_finished);
        }
        sql += " ORDER BY priority ASC, deadline ASC";

        std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(sql));
        pstmt->setInt(1, uid);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        while (res && res->next()) {
            TodoInfo info;
            info.id = res->getInt("id");
            info.uid = res->getInt("uid");
            info.content = res->getString("content");
            info.priority = res->getInt("priority");
            info.deadline = res->getString("deadline");
            info.is_finished = res->getInt("is_finished");
            todos.push_back(info);
        }
        return true;
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in listTodo: {}", exp.what());
        return false;
    }
}

bool MySQLDao::updateTodo(int id, int uid, const std::string& content, int priority,
                          const std::string& deadline, int is_finished) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        std::string sql = "UPDATE todo_list SET content = ?, priority = ?, deadline = ?, is_finished = ? "
                          "WHERE id = ? AND uid = ?";
        std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(sql));
        pstmt->setString(1, content);
        pstmt->setInt(2, priority);
        if (deadline.empty()) {
            pstmt->setNull(3, sql::DataType::TIMESTAMP);
        } else {
            pstmt->setString(3, deadline);
        }
        pstmt->setInt(4, is_finished);
        pstmt->setInt(5, id);
        pstmt->setInt(6, uid);
        return pstmt->executeUpdate() > 0;
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in updateTodo: {}", exp.what());
        return false;
    }
}

bool MySQLDao::deleteTodo(int id, int uid) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        std::string sql = "DELETE FROM todo_list WHERE id = ? AND uid = ?";
        std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(sql));
        pstmt->setInt(1, id);
        pstmt->setInt(2, uid);
        return pstmt->executeUpdate() > 0;
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in deleteTodo: {}", exp.what());
        return false;
    }
}

// 查找任务id为taskid，用户为uid对当前任务的状态
int MySQLDao::getAssigneeStatus(int taskId, int uid) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        std::string sql = "SELECT status FROM task_assignments WHERE task_id = ? AND assignee_uid = ?";
        std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(sql));
        pstmt->setInt(1, taskId);
        pstmt->setInt(2, uid);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res && res->next()) {
            return res->getInt("status");
        }
        return -1; 
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in getAssigneeStatus: {}", exp.what());
        return -1;
    }
}

std::vector<MySQLDao::DeadlineTask> MySQLDao::getDeadlineTasksToday() {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    std::vector<DeadlineTask> tasks;
    try {
        auto& sql_conn = connection.get()->getConn();
        std::string sql = "SELECT id, uid, title, assigned_to FROM task "
                          "WHERE DATE(deadline) = CURDATE() AND status != 2 AND assigned_to != ''";
        std::unique_ptr<sql::Statement> stmt(sql_conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(sql));
        while (res && res->next()) {
            DeadlineTask t;
            t.id = res->getInt("id");
            t.uid = res->getInt("uid");
            t.title = res->getString("title");
            t.assigned_to = res->getString("assigned_to");
            tasks.push_back(t);
        }
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in getDeadlineTasksToday: {}", exp.what());
    }
    return tasks;
}

std::vector<int> MySQLDao::getUncheckedInUsersToday() {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    std::vector<int> uids;
    try {
        auto& sql_conn = connection.get()->getConn();
        std::string sql = "SELECT uid FROM user WHERE status = 1 "
                          "AND uid NOT IN (SELECT uid FROM checkins WHERE checkin_date = CURDATE())";
        std::unique_ptr<sql::Statement> stmt(sql_conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(sql));
        while (res && res->next()) {
            uids.push_back(res->getInt("uid"));
        }
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in getUncheckedInUsersToday: {}", exp.what());
    }
    return uids;
}

int MySQLDao::checkin(int uid) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        // 直接 INSERT，利用 UNIQUE INDEX uk_uid_date 保证原子性
        // 如果今日已打卡，duplicate key 异常会被捕获并返回 -2
        std::string sql = "INSERT INTO checkins (uid, checkin_date) VALUES (?, CURDATE())";
        std::unique_ptr<sql::PreparedStatement> pstmt(sql_conn->prepareStatement(sql));
        pstmt->setInt(1, uid);
        pstmt->executeUpdate();

        std::unique_ptr<sql::Statement> stmt(sql_conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID() AS id"));
        if (res && res->next()) {
            return res->getInt("id");
        }
        return -1;
    } catch (const sql::SQLException& exp) {
        // MySQL errno 1062 = duplicate entry for unique key
        if(exp.getErrorCode() == 1062) {
            return -2;  // 此时说明今日已经打卡（唯一索引限制）
        }
        LOG_ERROR("SQLException in checkin: {}", exp.what());
        return -1;
    }
}

bool MySQLDao::getCheckins(int uid, const std::string& date_from, const std::string& date_to,
                           std::vector<CheckinRecord>& records) {
    auto connection = ConnectionGuard(*pool_, pool_->getConnection());
    try {
        auto& sql_conn = connection.get()->getConn();
        std::string sql =
            "SELECT uid, checkin_date, "
            "DATE_FORMAT(created_at,'%Y-%m-%d %H:%i:%s') AS created_at "
            "FROM checkins WHERE 1=1";
        if (uid > 0) {
            sql += " AND uid = " + std::to_string(uid);
        }
        if (!date_from.empty()) {
            sql += " AND checkin_date >= '" + date_from + "'";
        }
        if (!date_to.empty()) {
            sql += " AND checkin_date <= '" + date_to + "'";
        }
        sql += " ORDER BY checkin_date DESC";

        std::unique_ptr<sql::Statement> stmt(sql_conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(sql));
        while (res && res->next()) {
            CheckinRecord rec;
            rec.uid = res->getInt("uid");
            rec.checkin_date = res->getString("checkin_date");
            rec.created_at = res->getString("created_at");
            records.push_back(rec);
        }
        return true;
    } catch (const sql::SQLException& exp) {
        LOG_ERROR("SQLException in getCheckins: {}", exp.what());
        return false;
    }
}
