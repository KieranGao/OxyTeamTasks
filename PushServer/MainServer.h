#ifndef MAINSERVER_H
#define MAINSERVER_H

#include "Session.h"
#include <boost/asio.hpp>
#include "IOContextPool.h"
#include <memory>
#include <map>
#include <mutex>

using boost::asio::ip::tcp;

class MainServer {
public:
    MainServer(boost::asio::io_context& io_context, short port, const std::string& server_name);
    ~MainServer() = default;
    void clearSession(std::string uuid);
    void stop();
private:
    void onAccept(std::shared_ptr<Session>, beast::error_code ec);
    void doAccept();

    boost::asio::io_context& io_context_;
    short port_;
    std::string server_name_;
    tcp::acceptor acceptor_;
    std::map<std::string, std::shared_ptr<Session>> sessions_;
    std::mutex mtx_;
};

#endif /* MAINSERVER_H */
