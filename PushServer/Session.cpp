#include "Session.h"
#include "MainServer.h"
#include <iostream>
#include <sstream>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "LogicSystem.h"

Session::Session(boost::asio::io_context& io_context, MainServer* server):
	socket_(io_context), server_(server), is_running_(true),head_parsed_(false) {
	boost::uuids::uuid  a_uuid = boost::uuids::random_generator()();
	uuid_ = boost::uuids::to_string(a_uuid);
	recv_head_node_ = make_shared<MessageNode>(HEAD_TOTAL_LEN);
}
Session::~Session() {
	std::cerr << "~Session destruct" << endl;
}

tcp::socket& Session::getSocket() {
	return socket_;
}

std::string& Session::getUUID() {
	return uuid_;
}

void Session::start(){
	asyncReadHead(HEAD_TOTAL_LEN);
}

void Session::send(std::string msg, short msgid) {
	std::lock_guard<std::mutex> lock(send_mtx_);
	int send_que_size = send_que_.size();
	if (send_que_size > MAX_SENDQUE) {
		std::cerr << "session: " << uuid_ << " send que fulled, size is " << MAX_SENDQUE << endl;
		return;
	}

	send_que_.push(make_shared<SendNode>(msg.c_str(), msg.length(), msgid));
	if (send_que_size > 0) {
		return;
	}
	auto& MessageNode = send_que_.front();
	boost::asio::async_write(socket_, boost::asio::buffer(MessageNode->data_, MessageNode->total_len_),
		std::bind(&Session::handleWrite, this, std::placeholders::_1, sharedSelf()));
}

void Session::send(char* msg, short max_length, short msgid) {
	std::lock_guard<std::mutex> lock(send_mtx_);
	int send_que_size = send_que_.size();
	if (send_que_size > MAX_SENDQUE) {
		std::cerr << "session: " << uuid_ << " send que fulled, size is " << MAX_SENDQUE << endl;
		return;
	}

	send_que_.push(make_shared<SendNode>(msg, max_length, msgid));
	if (send_que_size>0) {
		return;
	}
	auto& MessageNode = send_que_.front();
	boost::asio::async_write(socket_, boost::asio::buffer(MessageNode->data_, MessageNode->total_len_), 
		std::bind(&Session::handleWrite, this, std::placeholders::_1, sharedSelf()));
}

void Session::close() {
	socket_.close();
	is_running_ = false;
}

std::shared_ptr<Session>Session::sharedSelf() {
	return shared_from_this();
}

void Session::asyncReadBody(int total_len)
{
	auto self = shared_from_this();
	asyncReadFull(total_len, [self, this, total_len](const boost::system::error_code& ec, std::size_t bytes_transfered) {
		try {
			if (ec) {
				std::cerr << "handle read failed, error is " << ec.message() << endl;
				close();
				server_->clearSession(uuid_);
				return;
			}

			if (bytes_transfered < total_len) {
				std::cerr << "read length not match, read [" << bytes_transfered << "] , total ["
					<< total_len<<"]" << endl;
				close();
				server_->clearSession(uuid_);
				return;
			}

			memcpy(recv_msg_node_->data_ , data_ , bytes_transfered);
			recv_msg_node_->cur_len_ += bytes_transfered;
			recv_msg_node_->data_[recv_msg_node_->total_len_] = '\0';
			cout << "receive data is " << recv_msg_node_->data_ << endl;
			//此处将消息投递到逻辑队列中
			LogicSystem::getInstance().postMsgToQue(make_shared<LogicNode>(shared_from_this(), recv_msg_node_));
			//继续监听头部接受事件
			asyncReadHead(HEAD_TOTAL_LEN);
		}
		catch (std::exception& e) {
			std::cerr << "Exception code is " << e.what() << endl;
		}
		});
}

void Session::asyncReadHead(int total_len)
{
	auto self = shared_from_this();
	asyncReadFull(HEAD_TOTAL_LEN, [self, this](const boost::system::error_code& ec, std::size_t bytes_transfered) {
		try {
			if (ec) {
				std::cerr << "handle read failed, error is " << ec.message() << endl;
				close();
			    server_->clearSession(uuid_);
				return;
			}

			if (bytes_transfered < HEAD_TOTAL_LEN) {
				std::cerr << "read length not match, read [" << bytes_transfered << "] , total ["
					<< HEAD_TOTAL_LEN << "]" << endl;
				close();
				server_->clearSession(uuid_);
				return;
			}

			recv_head_node_->Clear();
			memcpy(recv_head_node_->data_, data_, bytes_transfered);

			//获取头部MSGID数据
			short msg_id = 0;
			memcpy(&msg_id, recv_head_node_->data_, HEAD_ID_LEN);
			//网络字节序转化为本地字节序
			msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);
			std::cout << "msg_id is " << msg_id << endl;
			//id非法
			if (msg_id > MAX_LENGTH) {
				std::cerr << "invalid msg_id is " << msg_id << endl;
				server_->clearSession(uuid_);
				return;
			}
			short msg_len = 0;
			memcpy(&msg_len, recv_head_node_->data_ + HEAD_ID_LEN, HEAD_DATA_LEN);
			//网络字节序转化为本地字节序
			msg_len = boost::asio::detail::socket_ops::network_to_host_short(msg_len);
			std::cerr << "msg_len is " << msg_len << endl;

			//id非法
			if (msg_len > MAX_LENGTH) {
				std::cerr << "invalid data length is " << msg_len << endl;
				server_->clearSession(uuid_);
				return;
			}

			recv_msg_node_ = make_shared<RecvNode>(msg_len, msg_id);
			asyncReadBody(msg_len);
		}
		catch (std::exception& e) {
			std::cout << "Exception code is " << e.what() << endl;
		}
		});
}

void Session::handleWrite(const boost::system::error_code& error, std::shared_ptr<Session> shared_self) {
	//增加异常处理
	try {
		if (!error) {
			std::lock_guard<std::mutex> lock(send_mtx_);
			//cout << "send data " << send_que_.front()->data_+HEAD_LENGTH << endl;
			send_que_.pop();
			if (!send_que_.empty()) {
				auto& MessageNode = send_que_.front();
				boost::asio::async_write(socket_, boost::asio::buffer(MessageNode->data_, MessageNode->total_len_),
					std::bind(&Session::handleWrite, this, std::placeholders::_1, shared_self));
			}
		}
		else {
			std::cout << "handle write failed, error is " << error.message() << endl;
			close();
			server_->clearSession(uuid_);
		}
	}
	catch (std::exception& e) {
		std::cerr << "Exception code : " << e.what() << endl;
	}
	
}

//读取完整长度
void Session::asyncReadFull(std::size_t maxLength, std::function<void(const boost::system::error_code&, std::size_t)> handler )
{
	::memset(data_, 0, MAX_LENGTH);
	asyncReadLen(0, maxLength, handler);
}

//读取指定字节数
void Session::asyncReadLen(std::size_t read_len, std::size_t total_len, 
	std::function<void(const boost::system::error_code&, std::size_t)> handler)
{
	auto self = shared_from_this();
	socket_.async_read_some(boost::asio::buffer(data_ + read_len, total_len-read_len),
		[read_len, total_len, handler, self](const boost::system::error_code& ec, std::size_t  bytesTransfered) {
			if (ec) {
				// 出现错误，调用回调函数
				handler(ec, read_len + bytesTransfered);
				return;
			}

			if (read_len + bytesTransfered >= total_len) {
				//长度够了就调用回调函数
				handler(ec, read_len + bytesTransfered);
				return;
			}

			// 没有错误，且长度不足则继续读取
			self->asyncReadLen(read_len + bytesTransfered, total_len, handler);
	});
}

LogicNode::LogicNode(shared_ptr<Session>  session, 
	shared_ptr<RecvNode> recvnode): session_(session), recvnode_(recvnode) {
}