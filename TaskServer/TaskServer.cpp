#include "TaskGrpcServiceImpl.h"
#include "StatusGrpcClient.h"
#include "Global.h"
#include "Logger.h"
#include "ConfigManager.h"
#include <grpcpp/grpcpp.h>
#include <thread>
#include <chrono>
#include <atomic>

void RunServer() {
    auto& g_config = ConfigManager::getInstance();
    std::string host = g_config["TaskServer"]["host"];
    std::string port = g_config["TaskServer"]["port"];
    std::string addr = host + ":" + port;

    Logger::getInstance();
    LOG_INFO("TaskServer starting on {}", addr);
    Logger::getInstance().setRemoteFlushCallback([](const std::vector<LogEntry>& batch){
        ReportLogReq req;
        req.set_service("TaskServer");
        for(auto& e : batch) {
            auto* entry = req.add_entries();
            entry->set_level(Logger::levelToString(e.level));
            entry->set_message(e.message);
            entry->set_timestamp(e.timestamp);
            entry->set_service("TaskServer");
        }
        StatusGrpcClient::getInstance().reportLog(req);
    });

    std::atomic<bool> hb_running{true};
    std::thread hb_thread([&](){
        while(hb_running) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            if(!hb_running.load()) break;
            StatusGrpcClient::getInstance().heartbeat(host, port);
        }
    });

    TaskGrpcServiceImpl service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    LOG_INFO("TaskServer gRPC listening on {}", addr);

    server->Wait();

    hb_running = false;
    if (hb_thread.joinable()) hb_thread.join();
    LOG_INFO("TaskServer stopped");
}

int main() {
    try {
        RunServer();
    } catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
