#include "KafkaProducer.h"
#include "ConfigManager.h"
#include "Logger.h"

KafkaProducer::KafkaProducer() {
    auto& config = ConfigManager::getInstance();
    std::string brokers = config["Kafka"]["brokers"];
    default_topic_ = config["Kafka"]["topic"];
    if (default_topic_.empty()) default_topic_ = "logs";

    if (brokers.empty()) {
        LOG_ERROR("Kafka brokers not configured");
        return;
    }

    char errstr[512];
    rd_kafka_conf_t* conf = rd_kafka_conf_new();

    // 设置 broker 地址
    if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers.c_str(), errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        LOG_ERROR("Kafka conf set failed: {}", errstr);
        rd_kafka_conf_destroy(conf);
        return;
    }

    // 消息确认：1 表示 leader 确认即可
    rd_kafka_conf_set(conf, "acks", "1", errstr, sizeof(errstr));

    // 创建 producer
    rk_ = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!rk_) {
        LOG_ERROR("Kafka producer creation failed: {}", errstr);
        rd_kafka_conf_destroy(conf);
        return;
    }

    LOG_INFO("Kafka producer initialized, brokers={}", brokers);
}

KafkaProducer::~KafkaProducer() {
    if (rk_) {
        rd_kafka_flush(rk_, 5000); // 等待最多 5 秒发完剩余消息
        rd_kafka_destroy(rk_);
    }
}

bool KafkaProducer::produce(const std::string& topic, const std::string& key, const std::string& value) {
    if (!rk_) return false;

    // 查找或创建 topic handle
    rd_kafka_topic_t* rkt = rd_kafka_topic_new(rk_, topic.c_str(), nullptr);
    if (!rkt) {
        LOG_ERROR("Kafka topic creation failed: {}", rd_kafka_err2str(rd_kafka_last_error()));
        return false;
    }

    // RD_KAFKA_MSG_F_COPY: 让 rdkafka 复制消息内容，调用方无需保持 buffer
    int err = rd_kafka_produce(
        rkt,
        RD_KAFKA_PARTITION_UA,        // 自动分区
        RD_KAFKA_MSG_F_COPY,          // 复制消息
        const_cast<char*>(value.data()), value.size(),
        key.data(), key.size(),
        nullptr                        // opaque
    );

    rd_kafka_topic_destroy(rkt);

    if (err == -1) {
        LOG_ERROR("Kafka produce failed: {}", rd_kafka_err2str(rd_kafka_last_error()));
        return false;
    }

    // 触发回调处理（非阻塞）
    rd_kafka_poll(rk_, 0);
    return true;
}

namespace {
// JSON 字符串转义：替换 " 和 \ 等特殊字符
std::string kafkaJsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c; break;
        }
    }
    return out;
}
} // anonymous namespace

bool KafkaProducer::produceLogBatch(const std::string& service_name, const std::vector<LogEntry>& batch) {
    if (!rk_ || batch.empty()) return false;

    // 手动拼接 JSON（避免依赖 jsoncpp）
    std::string json = "{\"service\":\"" + kafkaJsonEscape(service_name) + "\",\"entries\":[";
    for (size_t i = 0; i < batch.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"level\":\"" + Logger::levelToString(batch[i].level)
              + "\",\"message\":\"" + kafkaJsonEscape(batch[i].message)
              + "\",\"timestamp\":" + std::to_string(batch[i].timestamp) + "}";
    }
    json += "]}";

    return produce(default_topic_, service_name, json);
}
