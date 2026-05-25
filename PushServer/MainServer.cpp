#include "MainServer.h"
#include <iostream>

MainServer::MainServer(boost::asio::io_context& io_context, short port)
    : io_context_(io_context), port_(port), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
    std::cerr << "PushServer listening on port: " << port_ << std::endl;
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
                    std::cerr << "WebSocket handshake failed: " << ws_ec.message() << std::endl;
                }
            });
    } else {
        std::cerr << "Accept failed: " << ec.message() << std::endl;
    }
    doAccept();
}

void MainServer::clearSession(std::string uuid) {
    std::lock_guard<std::mutex> lock(mtx_);
    sessions_.erase(uuid);
}
