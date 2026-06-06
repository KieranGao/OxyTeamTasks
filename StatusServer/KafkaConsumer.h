#ifndef KAFKACONSUMER_H
#define KAFKACONSUMER_H

#include <string>
#include <atomic>
#include <thread>
#include <functional>
#include <librdkafka/rdkafka.h>

class KafkaConsumer {
public:
    // message_callback: 收到消息后的处理函数，参数为 JSON 字符串
    KafkaConsumer(const std::string& brokers, const std::string& topic,
                  const std::string& group_id, std::function<void(const std::string&)> message_callback);
    ~KafkaConsumer();
    void start();
    void stop();
private:
    void consumeLoop();
    rd_kafka_t* rk_ = nullptr;
    rd_kafka_topic_partition_list_t* topics_ = nullptr;
    std::function<void(const std::string&)> callback_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

#endif /* KAFKACONSUMER_H */
