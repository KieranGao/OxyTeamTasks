#include "UMSGrpcServiceImpl.h"
#include "ConfigManager.h"
#include "Logger.h"
#include "StatusGrpcClient.h"
#include "KafkaProducer.h"
#include <grpcpp/grpcpp.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>

int main() {
    ConfigManager& config = ConfigManager::getInstance();
    std::string host = config["UMSServer"]["host"];
    std::string port = config["UMSServer"]["port"];
    std::string addr = host + ":" + port;

    Logger::getInstance();
    LOG_INFO("UMSServer starting on {}", addr);

    Logger::getInstance().setRemoteFlushCallback([](const std::vector<LogEntry>& batch) {
        KafkaProducer::getInstance().produceLogBatch("UMSServer", batch);
    });

    // Heartbeat thread
    std::atomic<bool> hb_running{true};
    std::thread hb_thread([&]() {
        while (hb_running) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            if (!hb_running) break;
            StatusGrpcClient::getInstance().heartbeat(host, port);
        }
    });

    UMSGrpcServiceImpl service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    LOG_INFO("UMSServer gRPC listening on {}", addr);

    server->Wait();

    hb_running = false;
    if (hb_thread.joinable()) hb_thread.join();
    return 0;
}
