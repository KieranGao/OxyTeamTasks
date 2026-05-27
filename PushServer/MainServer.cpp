#include "MainServer.h"
#include "AsyncTaskPool.h"
#include <iostream>
#include "Logger.h"
#include "StatusGrpcClient.h"

MainServer::MainServer(boost::asio::io_context& io_context, short port, const std::string& server_name)
    : io_context_(io_context), port_(port), server_name_(server_name), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
    LOG_DEBUG("PushServer listening on port: {}", port_);
    doAccept();
}

void MainServer::doAccept() {
    auto& io_context = IOContextPool::getInstance().getIOContext();
    auto session = std::make_shared<Session>(io_context, this);
    acceptor_.async_accept(session->getSocket(),
        std::bind(&MainServer::onAccept, this, session, std::placeholders::_1));
}

void MainServer::onAccept(std::shared_ptr<Session> session, beast::error_code ec) {
    if (!ec) {
        // Perform WebSocket handshake
        session->ws().async_accept(
            [this, session](beast::error_code ws_ec) {
                if (!ws_ec) {
                    std::lock_guard<std::mutex> lock(mtx_);
                    sessions_.insert({session->getUUID(), session});
                    session->start();
                } else {
                    LOG_ERROR("WebSocket handshake failed: {}", ws_ec.message());
                }
            });
    } else {
        LOG_ERROR("Accept failed: {}", ec.message());
    }
    doAccept();
}

void MainServer::stop() {
    // 先关闭所有活跃 session，再停 acceptor
    acceptor_.close();
    std::lock_guard<std::mutex> lock(mtx_);
    for (auto& [uuid, session] : sessions_) {
        session->close();
    }
    sessions_.clear();
}

void MainServer::clearSession(std::string uuid) {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        sessions_.erase(uuid);
    }
    LOG_DEBUG("[PushServer] session cleared: uuid={}", uuid);
    // 通知 StatusServer 递减连接计数 — 投递到线程池，不阻塞 IO 线程
    std::string name = server_name_;
    AsyncTaskPool::getInstance().post([name]() {
        try {
            StatusGrpcClient::getInstance().reportDisconnect(name);
        } catch (...) {
            LOG_ERROR("[PushServer] reportDisconnect failed for {}", name);
        }
    });
}
