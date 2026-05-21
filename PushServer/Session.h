#ifndef SESSION_H
#define SESSION_H

#include <boost/asio.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>

#include <queue>
#include <mutex>
#include <memory>
#include "Global.h"
#include "MessageNode.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class MainServer;
class LogicSystem;
class LogicNode {
	friend class LogicSystem;
public:
	LogicNode(std::shared_ptr<Session>, std::shared_ptr<RecvNode>);
private:
	std::shared_ptr<Session> session_;
	std::shared_ptr<RecvNode> recvnode_;
};
class Session : public std::enable_shared_from_this<Session>
{
public:
    Session(boost::asio::io_context& io_context, MainServer* server);
	~Session() = default;

	tcp::socket& getSocket();
	std::string& getUUID();
    
	void start();
	void send(char* msg,  short max_length, short msgid);
	void send(std::string msg, short msgid);
	void close();
	std::shared_ptr<Session> sharedSelf();
	void asyncReadBody(int length);
	void asyncReadHead(int total_len);
private:

	void asyncReadFull(std::size_t maxLength, std::function<void(const boost::system::error_code& , std::size_t)> handler);
	void asyncReadLen(std::size_t  read_len, std::size_t total_len, std::function<void(const boost::system::error_code&, std::size_t)> handler);
	void handleWrite(const boost::system::error_code& error, std::shared_ptr<Session> shared_self);

	tcp::socket socket_;
	std::string uuid_;
	char data_[MAX_LENGTH];
	MainServer* server_;
	bool is_running_;
	std::queue<std::shared_ptr<SendNode> > send_que_;
	std::mutex send_mtx_;
	//收到的消息结构
	std::shared_ptr<RecvNode> recv_msg_node_;
	bool head_parsed_;
	//收到的头部结构
	std::shared_ptr<MessageNode> recv_head_node_;
};

#endif /* SESSION_H */
