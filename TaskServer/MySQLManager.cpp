#include "MySQLManager.h"

MySQLManager::MySQLManager() {
    dao_ = std::make_unique<MySQLDao>();
}

int MySQLManager::createTask(int uid, const std::string& title, const std::string& description,
                             int priority, const std::string& deadline, const std::string& assigned_to) {
    return dao_->createTask(uid, title, description, priority, deadline, assigned_to);
}

bool MySQLManager::updateTask(int id, int uid, const std::string& title, const std::string& description,
                              int status, int priority, const std::string& deadline, const std::string& assigned_to) {
    return dao_->updateTask(id, uid, title, description, status, priority, deadline, assigned_to);
}

bool MySQLManager::deleteTask(int id, int uid) {
    return dao_->deleteTask(id, uid);
}

bool MySQLManager::getTask(int id, TaskInfo& info) {
    return dao_->getTask(id, info);
}

bool MySQLManager::listTasks(int uid, int status, const std::string& assigned_to, std::vector<TaskInfo>& tasks) {
    return dao_->listTasks(uid, status, assigned_to, tasks);
}

int MySQLManager::addTodo(int uid, const std::string& content, int priority, const std::string& deadline) {
    return dao_->addTodo(uid, content, priority, deadline);
}

bool MySQLManager::listTodo(int uid, int is_finished, std::vector<TodoInfo>& todos) {
    return dao_->listTodo(uid, is_finished, todos);
}

bool MySQLManager::updateTodo(int id, int uid, const std::string& content, int priority,
                              const std::string& deadline, int is_finished) {
    return dao_->updateTodo(id, uid, content, priority, deadline, is_finished);
}

bool MySQLManager::deleteTodo(int id, int uid) {
    return dao_->deleteTodo(id, uid);
}

int MySQLManager::getAssigneeStatus(int taskId, int uid) {
    return dao_->getAssigneeStatus(taskId, uid);
}

std::vector<MySQLDao::DeadlineTask> MySQLManager::getDeadlineTasksToday() {
    return dao_->getDeadlineTasksToday();
}

std::vector<int> MySQLManager::getUncheckedInUsersToday() {
    return dao_->getUncheckedInUsersToday();
}

int MySQLManager::checkin(int uid) {
    return dao_->checkin(uid);
}

bool MySQLManager::getCheckins(int uid, const std::string& date_from, const std::string& date_to,
                               std::vector<CheckinRecord>& records) {
    return dao_->getCheckins(uid, date_from, date_to, records);
}
