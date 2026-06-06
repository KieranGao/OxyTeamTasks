#include <iostream>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "Global.h"
#include "ConfigManager.h"
#include "Logger.h"
#include <hiredis/hiredis.h>
#include "RedisManager.h"
#include <memory>
#include <string>
#include <thread>
#include <boost/asio.hpp>
#include <grpcpp/grpcpp.h>
#include "StatusServiceImpl.h"
#include "KafkaConsumer.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::InsecureServerCredentials;

void RunServer() {
    auto& g_config = ConfigManager::getInstance();
    std::string host = g_config["StatusServer"]["host"];
    std::string port = g_config["StatusServer"]["port"];
    std::string server_address = host + ":" + port;

    Logger::getInstance();
    Logger::getInstance().setRemoteFlushCallback([](const std::vector<LogEntry>& batch) {
        LOG_DEBUG("[StatusServer] log remote flush callback called!");
        for(auto &entry : batch) {
            if (entry.level == LogLevel::DEBUG) continue;  // DEBUG 只写本地文件，不推远程
            std::string redis_key = "logs:StatusServer";
            std::string service = "StatusServer";
            std::string lvl = "INFO";
            switch(entry.level) {
                case LogLevel::INFO:{
                    lvl = "INFO";
                    break;
                }
                case LogLevel::ERROR: {
                    lvl = "ERROR";
                    break;
                }
                case LogLevel::WARN: {
                    lvl = "WARN";
                    break;
                }
                default:{
                    LOG_ERROR("Invalid log level!");
                    break;
                }
            }
            std::string json = "{\"service\":\"" + service + "\",\"level\":\"" + lvl 
                + "\",\"message\":\"" + jsonEscape(entry.message) + "\",\"timestamp\":" + std::to_string(entry.timestamp) + "}";

            auto& redis = RedisManager::getInstance();
            redis.lpush(redis_key, json);
            redis.ltrim(redis_key, 0, 499);
            redis.expire(redis_key, 604800);
        }
    });
    LOG_INFO("StatusServer starting on {}", server_address);

    // 启动 Kafka Consumer（日志消费主通道）
    std::string kafka_brokers = g_config["Kafka"]["brokers"];
    std::string kafka_topic = g_config["Kafka"]["topic"];
    if (kafka_topic.empty()) kafka_topic = "logs";

    KafkaConsumer kafka_consumer(kafka_brokers, kafka_topic, "status-server-log-group",
        [](const std::string& json_str) {
            // 解析 Kafka 消息 JSON，调用 storeLogEntries
            Json::Value root;
            Json::CharReaderBuilder builder;
            std::string errs;
            std::istringstream stream(json_str);
            if (!Json::parseFromStream(builder, stream, &root, &errs)) {
                LOG_ERROR("Kafka log JSON parse failed: {}", errs);
                return;
            }

            std::string service = root["service"].asString();
            std::vector<LogEntryData> entries;
            const auto& arr = root["entries"];
            for (Json::Value::ArrayIndex i = 0; i < arr.size(); ++i) {
                LogEntryData e;
                e.level = arr[i]["level"].asString();
                e.message = arr[i]["message"].asString();
                e.timestamp = arr[i]["timestamp"].asInt64();
                entries.push_back(std::move(e));
            }

            if (!entries.empty()) {
                StatusServiceImpl::storeLogEntries(service, entries);
            }
        });
    kafka_consumer.start();
    LOG_INFO("Kafka consumer started, topic={}", kafka_topic);

    StatusServiceImpl service;
    ServerBuilder builder;
    builder.AddListeningPort(server_address, InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    LOG_INFO("StatusServer gRPC listening on {}", server_address);

    boost::asio::io_context signal_io;
    boost::asio::signal_set signals(signal_io, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code& ec, int) {
        if (!ec) {
            LOG_INFO("StatusServer shutting down...");
            kafka_consumer.stop();
            server->Shutdown();
        }
    });

    std::thread signal_thread([&signal_io]() {
        signal_io.run();
    });

    server->Wait();
    signal_io.stop();
    if (signal_thread.joinable()) signal_thread.join();
    LOG_INFO("StatusServer stopped");
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
