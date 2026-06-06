#include "Global.h"
#include "MainServer.h"
#include "ConfigManager.h"
#include "Logger.h"
#include "StatusGrpcClient.h"
#include "KafkaProducer.h"
#include "AsyncTaskPool.h"
#include <hiredis/hiredis.h>
#include <thread>
#include <chrono>
#include "assert.h"
#include "RedisManager.h"

int main() {
    ConfigManager& configManager = ConfigManager::getInstance();
    std::string gate_port_str = configManager["GateServer"]["port"];
    unsigned short gate_port = std::atoi(gate_port_str.c_str());

    // 因为是懒汉式单例，调用时才初始化
    Logger::getInstance();

    Logger::getInstance().setRemoteFlushCallback([](const std::vector<LogEntry>& batch) {
        KafkaProducer::getInstance().produceLogBatch("GateServer", batch);
    });

    AsyncTaskPool::getInstance();  // 提前初始化线程池
    RedisManager::getInstance();   // 初始化 Redis 连接池（用于 token 验证等）
    LOG_INFO("GateServer starting on port {}", gate_port);

    // Heartbeat thread
    std::atomic<bool> hb_running{true};
    std::thread hb_thread([&hb_running, &configManager]() {
        std::string host = configManager["GateServer"]["host"];
        std::string port = configManager["GateServer"]["port"];
        while (hb_running) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            if (!hb_running) break;
            StatusGrpcClient::getInstance().heartbeat(host, port);
        }
    });

    try
    {
        unsigned short port = static_cast<unsigned short>(gate_port);
        net::io_context ioc{ 1 };
        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc, &hb_running](const boost::system::error_code& error, int signal_number) {
            if (error) return;
            hb_running = false;
            LOG_INFO("GateServer shutting down");
            AsyncTaskPool::getInstance().stop();   // 1. 停线程池（排空任务）
            Logger::getInstance().stop();           // 2. 停 logger flush
            ioc.stop();                             // 3. 停 IO
        });
        std::make_shared<MainServer>(ioc, port)->start();
        ioc.run();
    }
    catch (std::exception const& e)
    {
        LOG_ERROR("Fatal error: {}", e.what());
        hb_running = false;
        if (hb_thread.joinable()) hb_thread.join();
        return EXIT_FAILURE;
    }

    hb_running = false;
    if (hb_thread.joinable()) hb_thread.join();
    return 0;
}
