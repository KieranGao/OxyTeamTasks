#ifndef MAINSERVER_H
#define MAINSERVER_H

#include "Session.h"
#include <boost/asio.hpp>
#include "IOContextPool.h"
#include <memory>
#include <map>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>

using boost::asio::ip::tcp;

class MainServer {
public:
    MainServer(boost::asio::io_context& io_context, short port, const std::string& server_name);
    ~MainServer() = default;
    void clearSession(std::string uuid);
    void stop();
    void addUidSession(int uid, std::shared_ptr<Session> session);
    void removeUidSession(int uid);
    std::shared_ptr<Session> getSessionByUid(int uid);
    const std::string& getServerName() const { return server_name_; }
private:
    void onAccept(std::shared_ptr<Session>, beast::error_code ec);
    void doAccept();

    boost::asio::io_context& io_context_;
    short port_;
    std::string server_name_;
    tcp::acceptor acceptor_;
    std::map<std::string, std::shared_ptr<Session>> sessions_;
    std::mutex mtx_;
    std::unordered_map<int, std::shared_ptr<Session>> uid_sessions_;
    std::shared_mutex uid_mtx_;
};

#endif /* MAINSERVER_H */
