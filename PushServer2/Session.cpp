#include "Session.h"
#include "MainServer.h"
#include "LogicSystem.h"
#include <iostream>
#include "Logger.h"

Session::Session(net::io_context& io_context, MainServer* server)
    : ws_(io_context), server_(server), ping_timer_(io_context) {
    boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
    uuid_ = boost::uuids::to_string(a_uuid);
}

Session::~Session() {
    beast::error_code ec;
    ping_timer_.cancel(ec);
}

std::string& Session::getUUID() {
    return uuid_;
}

void Session::start() {
    // 握手完成后注册 pong 回调
    auto self = shared_from_this();
    ws_.control_callback([self](websocket::frame_type ft, boost::string_view) {
        if (ft == websocket::frame_type::pong) {
            self->pong_received_.store(true);
        }
    });
    doRead();
    startPingTimer();
}

void Session::send(const std::string& msg) {
    std::lock_guard<std::mutex> lock(send_mtx_);
    if (send_que_.size() > MAX_SENDQUE) {
        LOG_DEBUG("session: {} send queue full, size={}", uuid_, MAX_SENDQUE);
        return;
    }
    bool empty = send_que_.empty();
    send_que_.push(msg);
    if(empty) {
        doWriteLocked();  // send_mtx_ already held
    }
}

void Session::close() {
    if (!is_running_.exchange(false)) return;  // 原子操作，防止重复关闭
    LOG_DEBUG("[PushServer] session closing: uuid={}", uuid_);
    beast::error_code ec;
    ping_timer_.cancel(ec);
    // 用 socket 级别 shutdown 代替同步 ws_.close()，避免与 pending async_read 冲突
    ws_.next_layer().shutdown(tcp::socket::shutdown_both, ec);
}

std::shared_ptr<Session> Session::sharedSelf() {
    return shared_from_this();
}

void Session::doRead() {
    auto self = shared_from_this();
    ws_.async_read(read_buffer_, [self](beast::error_code ec, std::size_t bytes) {
        self->onRead(ec, bytes);
    });
}

void Session::onRead(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        LOG_ERROR("ws read error: {}", ec.message());
        close();
        server_->clearSession(uuid_);
        return;
    }

    std::string msg = beast::buffers_to_string(read_buffer_.data());
    read_buffer_.consume(bytes_transferred);
    LOG_DEBUG("ws recv: {}", msg);

    LogicSystem::getInstance().postMsgToQue(shared_from_this(), std::move(msg));
    doRead();
}

// 调用此方法前请先持有send_mtx_
void Session::doWriteLocked() {
    auto self = shared_from_this();
    if (send_que_.empty()) return;
    const std::string& msg = send_que_.front();
    ws_.async_write(net::buffer(msg), [self](beast::error_code ec, std::size_t bytes) {
        self->onWrite(ec, bytes);
    });
}

void Session::onWrite(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        LOG_ERROR("ws write error: {}", ec.message());
        close();
        server_->clearSession(uuid_);
        return;
    }
    std::lock_guard<std::mutex> lock(send_mtx_);
    send_que_.pop();
    if (!send_que_.empty()) {
        doWriteLocked();
    }
}

// ---- ping/pong 心跳 ----

void Session::startPingTimer() {
    auto self = shared_from_this();
    ping_timer_.expires_after(std::chrono::seconds(PING_INTERVAL_SECS));
    ping_timer_.async_wait([self](beast::error_code ec) {
        if (ec || !self->is_running_.load()) return;

        // 检查是否有写操作进行中，如果有则延迟到写完成后再 ping
        {
            std::lock_guard<std::mutex> lock(self->send_mtx_);
            if (!self->send_que_.empty()) {
                // 有写队列，延迟 1 秒后重试
                self->ping_timer_.expires_after(std::chrono::seconds(1));
                self->ping_timer_.async_wait([self](beast::error_code retry_ec) {
                    if (!retry_ec && self->is_running_.load()) {
                        self->startPingTimer();
                    }
                });
                return;
            }
        }

        // 无写操作，安全发送 ping
        self->pong_received_.store(false);
        self->ws_.async_ping(websocket::ping_data{}, [self](beast::error_code ping_ec) {
            if (ping_ec) {
                LOG_ERROR("[PushServer] ping send failed: {}", ping_ec.message());
                self->close();
                self->server_->clearSession(self->uuid_);
                return;
            }
            self->waitForPong();
        });
    });
}

void Session::waitForPong() {
    if (!is_running_.load()) return;

    auto self = shared_from_this();
    ping_timer_.expires_after(std::chrono::seconds(PONG_TIMEOUT_SECS));
    ping_timer_.async_wait([self](beast::error_code ec) {
        if (ec || !self->is_running_.load()) return;

        if (!self->pong_received_.load()) {
            LOG_WARN("[PushServer] pong timeout, closing session uuid={}", self->uuid_);
            self->close();
            self->server_->clearSession(self->uuid_);
        } else {
            self->startPingTimer();
        }
    });
}
