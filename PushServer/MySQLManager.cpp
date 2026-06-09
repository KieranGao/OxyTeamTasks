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

std::vector<int> MySQLManager::getUsersByTeam(int teamId) {
    return dao_->getUsersByTeam(teamId);
}

bool MySQLManager::insertMessage(int uid, const std::string& type, const std::string& title, const std::string& content) {
    return dao_->insertMessage(uid, type, title, content);
}

bool MySQLManager::markMessagesRead(int uid, const std::vector<int64_t>& ids) {
    return dao_->markMessagesRead(uid, ids);
}

bool MySQLManager::deleteMessages(int uid, const std::vector<int64_t>& ids) {
    return dao_->deleteMessages(uid, ids);
}

bool MySQLManager::getMessages(int uid, int offset, int limit, std::vector<MySQLDao::MessageRow>& messages) {
    return dao_->getMessages(uid, offset, limit, messages);
}

long MySQLManager::getUnreadCount(int uid) {
    return dao_->getUnreadCount(uid);
}