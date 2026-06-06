#include "TaskGrpcServiceImpl.h"
#include "StatusGrpcClient.h"
#include "KafkaProducer.h"
#include "PushGrpcClient.h"
#include "MySQLManager.h"
#include "Global.h"
#include "Logger.h"
#include "ConfigManager.h"
#include <grpcpp/grpcpp.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <set>
#include <sstream>

void RunServer() {
    auto& g_config = ConfigManager::getInstance();
    std::string host = g_config["TaskServer"]["host"];
    std::string port = g_config["TaskServer"]["port"];
    std::string addr = host + ":" + port;

    Logger::getInstance();
    LOG_INFO("TaskServer starting on {}", addr);
    Logger::getInstance().setRemoteFlushCallback([](const std::vector<LogEntry>& batch) {
        KafkaProducer::getInstance().produceLogBatch("TaskServer", batch);
    });

    std::atomic<bool> hb_running{true};
    std::thread hb_thread([&](){
        while(hb_running) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            if(!hb_running.load()) break;
            StatusGrpcClient::getInstance().heartbeat(host, port);
        }
    });
    

    // 任务提醒线程
    std::atomic<bool> reminder_running{true};
    std::set<std::string> triggered_dates;  
    std::thread reminder_thread([&](){
        while (reminder_running) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
            if (!reminder_running.load()) break;

            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            struct tm* tm_now = localtime(&time_t_now);
            char date_buf[16];
            strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", tm_now);
            std::string today(date_buf);
            int hour = tm_now->tm_hour;

            // 当日截止的任务，早8点提醒（整小时内触发一次）
            if (hour == 8 && triggered_dates.find(today + ":deadline") == triggered_dates.end()) {
                triggered_dates.insert(today + ":deadline");
                LOG_INFO("[Reminder] Running deadline reminder for {}", today);
                try {
                    auto tasks = MySQLManager::getInstance().getDeadlineTasksToday();
                    for (auto& t : tasks) {
                        std::istringstream ss(t.assigned_to);
                        std::string token;
                        std::string payload = "{\"task_id\":" + std::to_string(t.id) + ",\"task_title\":\"" + t.title + "\"}";
                        while (std::getline(ss, token, ',')) {
                            try {
                                int assignee = std::stoi(token);
                                PushGrpcClient::getInstance().pushToUser(assignee, "task_remind", "任务今日到期: " + t.title, payload);
                            } catch (...) {}
                        }
                    }
                } catch (...) {
                    LOG_ERROR("[Reminder] Deadline reminder failed");
                }
            }

            // 打卡提醒，晚8点提醒（整小时内触发一次）
            if (hour == 20 && triggered_dates.find(today + ":checkin") == triggered_dates.end()) {
                triggered_dates.insert(today + ":checkin");
                LOG_INFO("[Reminder] Running checkin reminder for {}", today);
                try {
                    auto uids = MySQLManager::getInstance().getUncheckedInUsersToday();
                    for (int uid : uids) {
                        PushGrpcClient::getInstance().pushToUser(uid, "checkin_remind", "今日尚未打卡，记得打卡哦！", "{}");
                    }
                } catch (...) {
                    LOG_ERROR("[Reminder] Checkin reminder failed");
                }
            }
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
    reminder_running = false;
    if (hb_thread.joinable()) hb_thread.join();
    if (reminder_thread.joinable()) reminder_thread.join();
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
