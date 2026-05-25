#ifndef SESSION_H
#define SESSION_H

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <queue>
#include <mutex>
#include <memory>
#include "Global.h"

class MainServer;
class LogicSystem;

class Session : public std::enable_shared_from_this<Session>
{
public:
    Session(net::io_context& io_context, MainServer* server);
    ~Session() = default;

    websocket::stream<tcp::socket>& ws() { return ws_; }
    tcp::socket& getSocket() { return ws_.next_layer(); }
    std::string& getUUID();
    void start();
    void send(const std::string& msg);
    void close();
    std::shared_ptr<Session> sharedSelf();
private:
    void doRead();
    void onRead(beast::error_code ec, std::size_t bytes_transferred);
    void doWrite();
    void onWrite(beast::error_code ec, std::size_t bytes_transferred);

    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer read_buffer_;
    std::string uuid_;
    MainServer* server_;
    bool is_running_;
    std::queue<std::string> send_que_;
    std::mutex send_mtx_;
};

#endif /* SESSION_H */
