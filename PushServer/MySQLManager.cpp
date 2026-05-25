#include "MySQLManager.h"

MySQLManager::MySQLManager() {
    dao_ = std::make_unique<MySQLDao>();
}

bool MySQLManager::checkLogin(const std::string& email, const std::string& password, UserInfo& userinfo) {
    return dao_->checkLogin(email, password, userinfo);
}

std::shared_ptr<UserInfo> MySQLManager::getUser(int uid) {
    auto ptr = dao_->getUser(uid);
    return ptr;
}