#ifndef MYSQLCONNECTPOOL_H
#define MYSQLCONNECTPOOL_H

#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <memory>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include "Global.h"

class SqlConnection {
public:
    SqlConnection(sql::Connection* connection, int64_t last_oper_time) : connection_(connection), last_oper_time_(last_oper_time) {}
    std::unique_ptr<sql::Connection>& getConn() {
        return this->connection_;
    }
    int64_t getLastTime() {
        return last_oper_time_;
    }
    void setLastTime(long long timeStamp) {
        last_oper_time_ = timeStamp;
    }
private:
    std::unique_ptr<sql::Connection> connection_;
    int64_t last_oper_time_;
};

class MySQLConnectPool {

public:
    MySQLConnectPool(size_t pool_size,std::string url, std::string user, std::string password, std::string dbName);
    ~MySQLConnectPool();
    void checkConnection();
    void checkLoop();
    std::unique_ptr<SqlConnection> getConnection();
    void returnConnection(std::unique_ptr<SqlConnection> connection);
    void stop();
private:
    std::string url_;
    std::string password_;
    std::string dbName_;
    std::string user_;
    int pool_size_;
    std::mutex mutex_;
    std::queue<std::unique_ptr<SqlConnection>> connections_;
    std::atomic<bool> is_running_;
    std::condition_variable cond_;
    std::thread check_thread_;
};

class ConnectionGuard {
public:
    ConnectionGuard(MySQLConnectPool& pool, std::unique_ptr<SqlConnection> conn)
        : pool_(pool), conn_(std::move(conn)) {}
    ~ConnectionGuard() {
        if (conn_) {
            pool_.returnConnection(std::move(conn_));
        }
    }
    ConnectionGuard(const ConnectionGuard&) = delete;
    ConnectionGuard& operator=(const ConnectionGuard&) = delete;
    ConnectionGuard(ConnectionGuard&&) = default;
    ConnectionGuard& operator=(ConnectionGuard&&) = default;

    std::unique_ptr<SqlConnection>& get() { return conn_; }
    explicit operator bool() const { return conn_ != nullptr; }
private:
    MySQLConnectPool& pool_;
    std::unique_ptr<SqlConnection> conn_;
};

#endif /* MYSQLCONNECTPOOL_H */
