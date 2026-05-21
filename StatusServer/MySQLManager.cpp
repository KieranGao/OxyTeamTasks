#include "MySQLManager.h"

MySQLManager::MySQLManager() {
    dao_ = std::make_unique<MySQLDao>();
}

int MySQLManager::registerUser(const std::string& username, const std::string& email, const std::string& password) {
    int ret = dao_->registerUser(username, email, password);
    return ret; 
}

bool MySQLManager::userResetpass(const std::string& username, const std::string& email, const std::string& new_password) {
    bool ret = dao_->userResetpass(username, email, new_password);
    return ret;
}

bool MySQLManager::checkLogin(const std::string& email, const std::string& password, UserInfo& userinfo) {
    bool ret = dao_->checkLogin(email, password, userinfo);
    return ret;
}