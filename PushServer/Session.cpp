#include "Session.h"
#include "MainServer.h"
#include "LogicSystem.h"
#include <iostream>
#include "Logger.h"

Session::Session(net::io_context& io_context, MainServer* server)
    : ws_(io_context), server_(server), is_running_(true) {
    boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
    uuid_ = boost::uuids::to_string(a_uuid);
}

std::string& Session::getUUID() {
    return uuid_;
}

void Session::start() {
    doRead();
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
    if (!is_running_) return;  // 防止 stop() 关闭后再被 onRead/onWrite 重复关闭
    is_running_ = false;
    LOG_DEBUG("[PushServer] session closing: uuid={}", uuid_);
    beast::error_code ec;
    ws_.close(websocket::close_code::normal, ec);
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
