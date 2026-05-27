#include "MainServer.h"
#include "IOContextPool.h"
#include "Logger.h"
MainServer::MainServer(boost::asio::io_context& ioc, unsigned short port)
    : acceptor_(ioc, tcp::endpoint(tcp::v4(), port)),
      ioc_(ioc) {
        LOG_DEBUG("GateServer is listening on port: {}", acceptor_.local_endpoint().port());
      }

void MainServer::start()
{    
    // 延长对象生命周期到回调执行时，否则在回调执行时对象可能已经被销毁了
    auto self = shared_from_this();
    auto& io_context_ = IOContextPool::getInstance().getIOContext();
    std::shared_ptr<HttpConnection> new_connection = std::make_shared<HttpConnection>(io_context_);
    acceptor_.async_accept(new_connection->socket(), [self, new_connection](beast::error_code ec) {
        try {
            //出错则放弃这个连接，继续监听新链接
            if (ec) {
                self->start();
                return;
            }
            //处理新链接
            new_connection->start();
            //继续监听
            self->start();
        }
        catch (std::exception& exp) {
            LOG_ERROR("exception is {}", exp.what());
            self->start();
        }
    });
}