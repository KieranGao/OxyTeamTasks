#include "KafkaConsumer.h"
#include "Logger.h"

KafkaConsumer::KafkaConsumer(const std::string& brokers, const std::string& topic,
                             const std::string& group_id,
                             std::function<void(const std::string&)> message_callback)
    : callback_(std::move(message_callback)) {

    char errstr[512];
    rd_kafka_conf_t* conf = rd_kafka_conf_new();

    // broker 地址
    if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers.c_str(), errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        LOG_ERROR("Kafka consumer conf set failed: {}", errstr);
        rd_kafka_conf_destroy(conf);
        return;
    }

    // 消费组
    if (rd_kafka_conf_set(conf, "group.id", group_id.c_str(), errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        LOG_ERROR("Kafka consumer group.id set failed: {}", errstr);
        rd_kafka_conf_destroy(conf);
        return;
    }

    // 从最早消息开始消费（首次加入组时）
    rd_kafka_conf_set(conf, "auto.offset.reset", "earliest", errstr, sizeof(errstr));

    // 自动提交 offset，每 5 秒
    rd_kafka_conf_set(conf, "enable.auto.commit", "true", errstr, sizeof(errstr));
    rd_kafka_conf_set(conf, "auto.commit.interval.ms", "5000", errstr, sizeof(errstr));

    // 创建 consumer
    rk_ = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
    if (!rk_) {
        LOG_ERROR("Kafka consumer creation failed: {}", errstr);
        rd_kafka_conf_destroy(conf);
        return;
    }

    // 订阅 topic
    topics_ = rd_kafka_topic_partition_list_new(1);
    rd_kafka_topic_partition_list_add(topics_, topic.c_str(), -1); // -1 = 所有分区

    LOG_INFO("Kafka consumer initialized, brokers={}, topic={}, group={}", brokers, topic, group_id);
}

KafkaConsumer::~KafkaConsumer() {
    stop();
    if (rk_) {
        rd_kafka_consumer_close(rk_);
        rd_kafka_destroy(rk_);
    }
    if (topics_) {
        rd_kafka_topic_partition_list_destroy(topics_);
    }
}

void KafkaConsumer::start() {
    if (!rk_) return;

    rd_kafka_resp_err_t err = rd_kafka_subscribe(rk_, topics_);
    if (err != RD_KAFKA_RESP_ERR_NO_ERROR) {
        LOG_ERROR("Kafka subscribe failed: {}", rd_kafka_err2str(err));
        return;
    }

    running_ = true;
    thread_ = std::thread(&KafkaConsumer::consumeLoop, this);
    LOG_INFO("Kafka consumer started");
}

void KafkaConsumer::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

void KafkaConsumer::consumeLoop() {
    while (running_) {
        rd_kafka_message_t* msg = rd_kafka_consumer_poll(rk_, 1000); // 1 秒超时
        if (!msg) continue;

        if (msg->err == RD_KAFKA_RESP_ERR_NO_ERROR) {
            // 成功收到消息
            std::string payload(static_cast<const char*>(msg->payload), msg->len);
            try {
                callback_(payload);
            } catch (const std::exception& e) {
                LOG_ERROR("Kafka message processing error: {}", e.what());
            }
        } else if (msg->err != RD_KAFKA_RESP_ERR__PARTITION_EOF) {
            LOG_ERROR("Kafka consumer error: {}", rd_kafka_message_errstr(msg));
        }

        rd_kafka_message_destroy(msg);
    }
}
