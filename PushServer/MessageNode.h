#ifndef MESSAGENODE_H
#define MESSAGENODE_H

#include <string>
#include "Global.h"
#include <iostream>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class LogicSystem;

class MessageNode {
public:
	MessageNode(short max_len) :total_len_(max_len), cur_len_(0) {
		data_ = new char[total_len_ + 1]();
		data_[total_len_] = '\0';
	}

	~MessageNode() {
		std::cerr << "destruct MessageNode" << endl;
		delete[] data_;
	}

	void Clear() {
		::memset(data_, 0, total_len_);
		cur_len_ = 0;
	}

	short cur_len_;
	short total_len_;
	char* data_;
};

class RecvNode :public MessageNode {
	friend class LogicSystem;
public:
	RecvNode(short max_len, short msg_id);
private:
	short msg_id_;
};

class SendNode:public MessageNode {
	friend class LogicSystem;
public:
	SendNode(const char* msg,short max_len, short msg_id);
private:
	short msg_id_;
};

#endif /* MESSAGENODE_H */
