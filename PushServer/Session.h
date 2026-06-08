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
#include <atomic>
#include "Global.h"

class MainServer;
class LogicSystem;

class Session : public std::enable_shared_from_this<Session>
{
public:
    Session(net::io_context& io_context, MainServer* server);
    ~Session();

    websocket::stream<tcp::socket>& ws() { return ws_; }
    tcp::socket& getSocket() { return ws_.next_layer(); }
    std::string& getUUID();
    void start();
    void send(const std::string& msg);
    void close();
    std::shared_ptr<Session> sharedSelf();
    void setUid(int uid) { uid_ = uid; }
    int getUid() const { return uid_; }
    MainServer* getServer() const { return server_; }
private:
    void doRead();
    void onRead(beast::error_code ec, std::size_t bytes_transferred);
    void doWriteLocked();   // caller must hold send_mtx_
    void onWrite(beast::error_code ec, std::size_t bytes_transferred);

    // ping/pong 心跳
    void startPingTimer();
    void waitForPong();

    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer read_buffer_;
    std::string uuid_;
    int uid_ = 0; // 每个会话维护当前会话用户的uid
    MainServer* server_;
    std::atomic<bool> is_running_{true};
    std::queue<std::string> send_que_; // 用于防止异步写过程中产生并发写冲突
    std::mutex send_mtx_;

    // ping/pong 心跳状态
    net::steady_timer ping_timer_;
    std::atomic<bool> pong_received_{false};
    static constexpr int PING_INTERVAL_SECS = 30;  // 每30秒发一次ping
    static constexpr int PONG_TIMEOUT_SECS = 10;   // 10秒内没收到pong则判定断线
};

#endif /* SESSION_H */
