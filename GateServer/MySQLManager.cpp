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

bool MySQLManager::updateTeamInfo(int uid, int belong_team_id) {
    return dao_->updateTeamInfo(uid, belong_team_id);
}

bool MySQLManager::getUserInfo(int uid, UserInfo& userinfo) {
    return dao_->getUserInfo(uid, userinfo);
}

std::vector<int> MySQLManager::getUsersByRole(int role) {
    return dao_->getUsersByRole(role);
}

bool MySQLManager::listMessages(int uid, int page, int pageSize, std::vector<MySQLDao::MessageRow>& messages, int& total) {
    return dao_->listMessages(uid, page, pageSize, messages, total);
}

bool MySQLManager::markMessagesRead(int uid, const std::vector<int64_t>& ids) {
    return dao_->markMessagesRead(uid, ids);
}

bool MySQLManager::deleteMessages(int uid, const std::vector<int64_t>& ids) {
    return dao_->deleteMessages(uid, ids);
}

bool MySQLManager::insertMessage(int uid, const std::string& type, const std::string& title, const std::string& content) {
    return dao_->insertMessage(uid, type, title, content);
}