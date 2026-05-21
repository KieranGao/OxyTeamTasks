#include "MainServer.h"
#include <iostream>

MainServer::MainServer(boost::asio::io_context& io_context, short port)
    : io_context_(io_context), port_(port), acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
{
    std::cerr << "Server started! Listening on port:" << port_ << std::endl;
    startAccept();
};

void MainServer::startAccept() {
    auto& io_context = IOContextPool::getInstance().getIOContext();
    std::shared_ptr<Session> new_session = std::make_shared<Session>(io_context, this);
    acceptor_.async_accept(new_session->getSocket(), std::bind(MainServer::handleAccept, this, new_session, std::placeholders::_1));
}

void MainServer::handleAccept(std::shared_ptr<Session> new_session, const boost::system::error_code& error) {
    if(!error) {
        new_session->start();
        std::lock_guard<std::mutex> lock(mtx_);
        sessions_.insert({new_session->getUUID(), new_session});
    } else {
        std::cerr << "Session accept failed, error code: " << error.message() << std::endl;
    }
    startAccept();
};

void MainServer::clearSession(std::string uuid) {
    std::lock_guard<std::mutex> lock(mtx_);
    sessions_.erase(uuid);
};
