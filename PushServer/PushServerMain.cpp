#include "LogicSystem.h"
#include "AsyncTaskPool.h"
#include "Logger.h"
#include "StatusGrpcClient.h"
#include <csignal>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include "IOContextPool.h"
#include "ConfigManager.h"
#include "MainServer.h"
#include <condition_variable>

std::atomic<bool> is_running{false};
std::condition_variable cv;
std::mutex quit_mtx;

int main() {
    try {
        auto& g_config = ConfigManager::getInstance();
        std::string host = g_config["SelfServer"]["host"];
        std::string port = g_config["SelfServer"]["port"];
        std::string identity = host + ":" + port;
        std::string log_service = "PushServer(" + identity + ")";

        // Init Logger FIRST, then set callback BEFORE any LOG calls
        Logger::getInstance();

        Logger::getInstance().setRemoteFlushCallback([log_service](const std::vector<LogEntry>& batch) {
            ReportLogReq req;
            req.set_service(log_service);
            for (auto& e : batch) {
                auto* entry = req.add_entries();
                entry->set_service(log_service);
                entry->set_level(Logger::levelToString(e.level));
                entry->set_message(e.message);
                entry->set_timestamp(e.timestamp);
            }
            StatusGrpcClient::getInstance().reportLog(req);
        });

        AsyncTaskPool::getInstance();  // 提前初始化线程池
        LOG_INFO("PushServer starting on {}", identity);

        // Heartbeat thread
        std::atomic<bool> hb_running{true};
        std::thread hb_thread([&]() {
            while (hb_running) {
                std::this_thread::sleep_for(std::chrono::seconds(10));
                if(!hb_running) break;
                StatusGrpcClient::getInstance().heartbeat(host, port);
            }
        });

        auto& pool = IOContextPool::getInstance();
        boost::asio::io_context io_context;
        auto server = std::make_shared<MainServer>(io_context, atoi(port.c_str()), identity);
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&io_context, &pool, &hb_running, server](const boost::system::error_code&, int){
            hb_running = false;
            LOG_INFO("PushServer shutting down...");
            Logger::getInstance().stop();          // 1. 停 logger flush
            server->stop();                        // 2. 关 session + acceptor（触发 disconnect 投递到线程池）
            AsyncTaskPool::getInstance().stop();   // 3. 排空 disconnect 任务
            io_context.stop();                     // 4. 停事件循环
            pool.stop();                           // 5. 停 IO 线程池
        });
        LOG_INFO("PushServer WebSocket listening on port {}", port);
        io_context.run();

        hb_running = false;
        if (hb_thread.joinable()) hb_thread.join();
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("Exception: {}", e.what());
    }
    return 0;
}
