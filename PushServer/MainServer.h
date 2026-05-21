#ifndef MAINSERVER_H
#define MAINSERVER_H

#include "Session.h"
#include <boost/asio.hpp>
#include "IOContextPool.h"
#include "Session.h"
#include <memory>
#include <map>
#include <mutex>

using boost::asio::ip::tcp;

class MainServer {
public:
    MainServer(boost::asio::io_context& io_context, short port);
    ~MainServer() = default;
    void clearSession(std::string session);
private:
    void handleAccept(std::shared_ptr<Session>, const boost::system::error_code& error);
    void startAccept();

    boost::asio::io_context& io_context_;
    short port_;
    tcp::acceptor acceptor_;
    std::map<std::string, std::shared_ptr<Session>> sessions_;
    std::mutex mtx_;
};

#endif /* MAINSERVER_H */