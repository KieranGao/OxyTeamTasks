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

bool MySQLManager::updateTeamInfo(int uid, int belong_captain_id) {
    return dao_->updateTeamInfo(uid, belong_captain_id);
}

bool MySQLManager::updateUserTeam(int uid, int belong_team_id) {
    return dao_->updateUserTeam(uid, belong_team_id);
}

bool MySQLManager::listPendingUsers(std::vector<PendingUserInfo>& users) {
    return dao_->listPendingUsers(users);
}

bool MySQLManager::approveUser(int uid, int role, int belong_team_id) {
    return dao_->approveUser(uid, role, belong_team_id);
}

bool MySQLManager::rejectUser(int uid) {
    return dao_->rejectUser(uid);
}

bool MySQLManager::setUserRole(int uid, int role, int belong_team_id) {
    return dao_->setUserRole(uid, role, belong_team_id);
}

bool MySQLManager::listAllUsers(std::vector<PendingUserInfo>& users) {
    return dao_->listAllUsers(users);
}