#include "LogicSystem.h"
#include "StatusGrpcClient.h"
#include "MySQLManager.h"
#include "Global.h"

LogicSystem::LogicSystem(): is_running_(true) {
	registerCallBacks();
	workers_thread_ = std::thread (&LogicSystem::dealMsg, this);
}

LogicSystem::~LogicSystem(){
	is_running_ = false;
	cv_.notify_one();
	workers_thread_.join();
}

void LogicSystem::postMsgToQue(std::shared_ptr <LogicNode> msg) {
	std::unique_lock<std::mutex> lock(mtx_);
	msg_queue_.push(msg);
	if (msg_queue_.size() == 1) {
		lock.unlock();
		cv_.notify_one();
	}
}

void LogicSystem::dealMsg() {
	for (;;) {
		std::unique_lock<std::mutex> lock(mtx_);

		cv_.wait(lock, [this]() {
			return !msg_queue_.empty() || !is_running_.load();
		});

		if (!is_running_) {
			break;
		}

		while (!msg_queue_.empty()) {
			auto msg_node = msg_queue_.front();
			std::cout << "recv_msg id is " << msg_node->recvnode_->msg_id_ << std::endl;
			auto call_back_iter = fun_callbacks_.find(msg_node->recvnode_->msg_id_);
			if (call_back_iter == fun_callbacks_.end()) {
				msg_queue_.pop();
				std::cout << "msg id [" << msg_node->recvnode_->msg_id_ << "] handler not found" << std::endl;
				continue;
			}
			call_back_iter->second(msg_node->session_, msg_node->recvnode_->msg_id_,
				std::string(msg_node->recvnode_->data_, msg_node->recvnode_->cur_len_));
			msg_queue_.pop();
		}
	}
}

void LogicSystem::registerCallBacks() {
	fun_callbacks_[MSG_CHAT_LOGIN] = std::bind(&LogicSystem::loginHandler, this,
		placeholders::_1, placeholders::_2, placeholders::_3);
}

void LogicSystem::loginHandler(shared_ptr<Session> session, const short &msg_id, const string &msgdata_) {
	Json::Reader reader;
	Json::Value root;
	reader.parse(msgdata_, root);
	auto uid = root["uid"].asInt();
	std::cout << "user login uid is  " << uid << " user token  is "
		<< root["token"].asString() << endl;
	auto rsp = StatusGrpcClient::getInstance().Login(uid, root["token"].asString());
	Json::Value  rtvalue;
	Defer defer([this, &rtvalue, session]() {
		std::string return_str = rtvalue.toStyledString();
		session->send(return_str, MSG_CHAT_LOGIN_RSP);
	});
	rtvalue["error"] = static_cast<int>(rsp.error());
	if (rsp.error() != static_cast<int>(ErrorCodes::SUCCESS)) {
		return;
	}
	auto find_iter = users_.find(uid);
	std::shared_ptr<UserInfo> user_info = nullptr;
	if(find_iter == users_.end()) {
		user_info = MySQLManager::getInstance().getUser(uid);
		if (user_info == nullptr) {
			rtvalue["error"] = static_cast<int>(ErrorCodes::USER_ID_INVALID);
			return;
		}
		users_[uid] = user_info;
	}
	else {
		user_info = find_iter->second;
	}
	rtvalue["uid"] = uid;
	rtvalue["token"] = rsp.token();
	rtvalue["name"] = user_info->name;
}