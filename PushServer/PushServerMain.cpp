#include "LogicSystem.h"
#include "AsyncTaskPool.h"
#include "Logger.h"
#include "StatusGrpcClient.h"
#include "KafkaProducer.h"
#include "PushGrpcServiceImpl.h"
#include "RPCConnectPool.h"
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
            KafkaProducer::getInstance().produceLogBatch(log_service, batch);
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

        setGlobalMainServer(server.get());
        // 另用一个端口用于grpc通信，上面的端口已被websocket占用
        std::string grpc_port = g_config["SelfServer"]["gRPC_port"];
        setGlobalGrpcPort(grpc_port);
        std::string grpc_addr = "0.0.0.0:" + grpc_port;
        PushGrpcServiceImpl pushServiceImpl;
        grpc::ServerBuilder grpc_builder;
        grpc_builder.AddListeningPort(grpc_addr, grpc::InsecureServerCredentials());
        grpc_builder.RegisterService(&pushServiceImpl);
        auto grpc_server = grpc_builder.BuildAndStart();

        LOG_INFO("PushService gRPC listening on {}", grpc_addr);

        std::thread grpc_thread([&grpc_server]() { grpc_server->Wait(); });

        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);

        signals.async_wait([&io_context, &pool, &hb_running, server, &grpc_server](const boost::system::error_code&, int){
            hb_running = false;
            LOG_INFO("PushServer shutting down...");
            grpc_server->Shutdown();
            Logger::getInstance().stop();
            server->stop();
            AsyncTaskPool::getInstance().stop();
            PushNodeManager::getInstance().stop();
            io_context.stop();
            pool.stop();
        });
        
        LOG_INFO("PushServer WebSocket listening on port {}", port);
        io_context.run();

        hb_running = false;
        if (hb_thread.joinable()) hb_thread.join();
        if (grpc_thread.joinable()) grpc_thread.join();
    }
    catch(const std::exception& e)
    {
        LOG_ERROR("Exception: {}", e.what());
    }
    return 0;
}
