#ifndef MAINSERVER_H
#define MAINSERVER_H

#include "Global.h"
#include "HttpConnection.h"

class MainServer : public std::enable_shared_from_this<MainServer>
{
public:
    // iocontext 底层实现了一个事件循环(epoll)，负责监听处理异步操作的事件。
    MainServer(boost::asio::io_context& ioc, unsigned short port);
    void start();
    
private:
    tcp::acceptor acceptor_;
    boost::asio::io_context& ioc_;
    // tcp::socket socket_; 多线程下socket对象不能共享，所以每次接受新连接时都需要创建一个新的socket对象
    
};

#endif /* MAINSERVER_H */
