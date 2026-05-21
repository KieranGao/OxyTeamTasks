#include <iostream>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "Global.h"
#include "ConfigManager.h"
#include <hiredis/hiredis.h>
#include "RedisManager.h"
#include "MySQLManager.h"
#include "IOContextPool.h"
#include <memory>
#include <string>
#include <thread>
#include <boost/asio.hpp>
#include <grpcpp/grpcpp.h>
#include "StatusServiceImpl.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::InsecureServerCredentials;

void RunServer() {
    auto& g_config = ConfigManager::getInstance();
    std::string server_address = g_config["StatusServer"]["host"] 
        + ":" + g_config["StatusServer"]["port"];
    // 初始化 gRPC 服务
    StatusServiceImpl service;
    ServerBuilder builder;
    builder.AddListeningPort(server_address, InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cerr << "Server listening on " << server_address << std::endl;
    // Boost.Asio 处理信号（SIGINT / SIGTERM）用于优雅关闭
    boost::asio::io_context io_context;
    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    // 信号捕获回调
    signals.async_wait([&](const boost::system::error_code& ec, int) {
        if (!ec) {
            std::cerr << "\nReceived shutdown signal, stopping gRPC server..." << std::endl;
            server->Shutdown(); // 关闭 gRPC 服务
        }
    });

    // 在独立线程运行 io_context
    std::thread signal_thread([&io_context]() {
        try {
            io_context.run();
        } catch (const std::exception& e) {
            std::cerr << "Signal thread error: " << e.what() << std::endl;
        }
    });

    // 等待 gRPC 服务器退出
    server->Wait();
    // 停止信号处理并等待线程退出
    io_context.stop();
    if (signal_thread.joinable()) {
        signal_thread.join();
    }

    std::cerr << "Status server stopped gracefully" << std::endl;
}

int main(int argc, char** argv) {
    try {
        RunServer();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return 0;
}