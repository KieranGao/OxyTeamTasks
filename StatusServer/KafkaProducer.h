#ifndef KAFKAPRODUCER_H
#define KAFKAPRODUCER_H

#include "Singleton.h"
#include <string>
#include <vector>
#include <librdkafka/rdkafka.h>

struct LogEntry;

class KafkaProducer : public Singleton<KafkaProducer> {
    friend class Singleton<KafkaProducer>;
public:
    ~KafkaProducer();
    // 发送消息到 Kafka，key 用于分区路由，value 为消息内容
    bool produce(const std::string& topic, const std::string& key, const std::string& value);
    // 批量日志序列化为 JSON 并发送
    bool produceLogBatch(const std::string& service_name, const std::vector<LogEntry>& batch);
private:
    KafkaProducer();
    rd_kafka_t* rk_ = nullptr;
    std::string default_topic_;
};

#endif /* KAFKAPRODUCER_H */
