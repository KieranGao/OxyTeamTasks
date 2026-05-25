#include "Session.h"
#include "MainServer.h"
#include "LogicSystem.h"
#include <iostream>

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
        std::cerr << "session: " << uuid_ << " send queue full, size=" << MAX_SENDQUE << std::endl;
        return;
    }
    bool was_empty = send_que_.empty();
    send_que_.push(msg);
    if (was_empty) {
        doWrite();
    }
}

void Session::close() {
    is_running_ = false;
    beast::error_code ec;
    ws_.close(websocket::close_code::normal, ec);
}

std::shared_ptr<Session> Session::sharedSelf() {
    return shared_from_this();
}

// ---- WebSocket async read ----

void Session::doRead() {
    auto self = shared_from_this();
    ws_.async_read(read_buffer_, [self](beast::error_code ec, std::size_t bytes) {
        self->onRead(ec, bytes);
    });
}

void Session::onRead(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        std::cerr << "ws read error: " << ec.message() << std::endl;
        close();
        server_->clearSession(uuid_);
        return;
    }

    std::string msg = beast::buffers_to_string(read_buffer_.data());
    read_buffer_.consume(bytes_transferred);
    std::cout << "ws recv: " << msg << std::endl;

    // Post JSON string to LogicSystem
    LogicSystem::getInstance().postMsgToQue(shared_from_this(), std::move(msg));

    // Continue reading
    doRead();
}

// ---- WebSocket async write (queue-based, sequential) ----

void Session::doWrite() {
    auto self = shared_from_this();
    std::lock_guard<std::mutex> lock(send_mtx_);
    if (send_que_.empty()) return;
    const std::string& msg = send_que_.front();
    ws_.async_write(net::buffer(msg), [self](beast::error_code ec, std::size_t bytes) {
        self->onWrite(ec, bytes);
    });
}

void Session::onWrite(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        std::cerr << "ws write error: " << ec.message() << std::endl;
        close();
        server_->clearSession(uuid_);
        return;
    }
    std::lock_guard<std::mutex> lock(send_mtx_);
    send_que_.pop();
    if (!send_que_.empty()) {
        doWrite();
    }
}
