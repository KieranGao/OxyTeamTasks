#include "MySQLConnectPool.h"
#include "Logger.h"
#include <chrono>
#include <chrono>

MySQLConnectPool::MySQLConnectPool(size_t pool_size,std::string url, std::string user, std::string password, std::string dbName) :
    pool_size_(pool_size), url_(std::move(url)), user_(std::move(user)), password_(std::move(password)), dbName_(std::move(dbName))
{
    is_running_.store(true);
    try {
        for(int i=1;i<=pool_size_;i++) {
            sql::mysql::MySQL_Driver* driver = sql::mysql::get_driver_instance();
            sql::Connection* connection =  driver->connect(url_, user_, password_);
            connection->setSchema(dbName_);
            auto curTime = std::chrono::steady_clock::now().time_since_epoch();
            long long timeStamp = std::chrono::duration_cast<std::chrono::seconds>(curTime).count();
            connections_.emplace(std::make_unique<SqlConnection>(connection, timeStamp));
        }
        check_thread_ = std::thread(&MySQLConnectPool::checkLoop, this);
    } catch(sql::SQLException& exp) {
        LOG_ERROR("SQL Initialize Error! {}", exp.what());
    }
}

MySQLConnectPool::~MySQLConnectPool() {
    stop();
    LOG_DEBUG("MySQL Connection Pool Destroyed!");
}

void MySQLConnectPool::checkLoop() {
    while(is_running_.load()) {
        checkConnection();
        // 该线程每分钟检查一下失效连接
        std::this_thread::sleep_for(std::chrono::seconds(60)); 
    }
}

void MySQLConnectPool::stop() {

    std::lock_guard<std::mutex> lock(mutex_);
    is_running_ = false;
    cond_.notify_all();
    if (check_thread_.joinable()) {
        check_thread_.join();
    }
    while (!connections_.empty()) {
        connections_.pop();
    }
}

void MySQLConnectPool::checkConnection() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto curTime = std::chrono::steady_clock::now().time_since_epoch();
    long long timeStamp = std::chrono::duration_cast<std::chrono::seconds>(curTime).count();
    for(int i=0;i<pool_size_;i++) {
        auto connection = std::move(connections_.front());
        connections_.pop();
        Defer defer([this, &connection]() {
            connections_.push(std::move(connection));
        });
        // 如果五分钟内有操作，不会断开连接
        if(timeStamp - connection->getLastTime() < 300) {
            continue;
        } 
        try {
            std::unique_ptr<sql::Statement> statement(connection->getConn()->createStatement());
            statement->executeQuery("SELECT 1");
            connection->setLastTime(timeStamp);
        } catch(sql::SQLException& exp) {
            LOG_ERROR("Error keeping connection alive: {}", exp.what());
            // 重新创建新的连接，替换旧的连接
            sql::mysql::MySQL_Driver* driver = sql::mysql::get_driver_instance();
            sql::Connection* new_connection = driver->connect(url_, user_, password_);
            new_connection->setSchema(dbName_);
            connection->getConn().reset(new_connection);
            connection->setLastTime(timeStamp);
        } 
    }
}

std::unique_ptr<SqlConnection> MySQLConnectPool::getConnection() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cond_.wait_for(lock, std::chrono::seconds(3), [this](){ return !connections_.empty() or !is_running_;})) {
        return nullptr;
    }
    if(!is_running_) return nullptr;
    auto connection = std::move(connections_.front());
    connections_.pop();
    return connection;
}

void MySQLConnectPool::returnConnection(std::unique_ptr<SqlConnection> connection) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(is_running_) {
        connections_.push(std::move(connection));
        cond_.notify_one();
    }
}