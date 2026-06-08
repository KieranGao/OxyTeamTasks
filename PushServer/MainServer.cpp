#include "MainServer.h"
#include "AsyncTaskPool.h"
#include <iostream>
#include "Logger.h"
#include "StatusGrpcClient.h"
#include "RedisManager.h"
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/random_generator.hpp"
#include "boost/uuid/uuid_io.hpp"

static std::string generate_lock_owner() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}

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
        // 进行WebSocket握手
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
    // 清除会话
    int uid = 0;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = sessions_.find(uuid);
        if (it != sessions_.end()) {
            uid = it->second->getUid();
            sessions_.erase(it);
        }
    }
    if (uid > 0) {
        removeUidSession(uid);
    }
    LOG_DEBUG("[PushServer] session cleared: uuid={} uid={}", uuid, uid);
    // 只有已认证的连接(uid>0)才上报断线，避免未认证连接导致计数变负
    if (uid > 0) {
        std::string name = server_name_;
        AsyncTaskPool::getInstance().post([name, uid]() {
            try {
                StatusGrpcClient::getInstance().reportDisconnect(name, uid);
            } catch (...) {
                LOG_ERROR("[PushServer] reportDisconnect failed for {}", name);
            }
        });
    }
}

void MainServer::addUidSession(int uid, std::shared_ptr<Session> session) {
    // uniquelock独占读写锁，此处进行写操作
    std::unique_lock<std::shared_mutex> lock(uid_mtx_);
    uid_sessions_[uid] = session;
    LOG_DEBUG("[PushServer] addUidSession: uid={}", uid);
}

void MainServer::removeUidSession(int uid) {
    std::unique_lock<std::shared_mutex> lock(uid_mtx_);
    uid_sessions_.erase(uid);
    LOG_DEBUG("[PushServer] removeUidSession: uid={}", uid);
}

std::shared_ptr<Session> MainServer::getSessionByUid(int uid) {
    // 只读
    std::shared_lock<std::shared_mutex> lock(uid_mtx_);
    auto it = uid_sessions_.find(uid);
    if (it != uid_sessions_.end()) return it->second;
    return nullptr;
}
