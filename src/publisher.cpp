// src/publisher.cpp
#include "sparkplug/publisher.hpp"
#include <MQTTAsync.h>
#include <format>
#include <thread>

namespace sparkplug {

Publisher::Publisher(Config config)
    : config_(std::move(config)), client_(nullptr) {}

Publisher::~Publisher() {
  if (client_) {
    MQTTAsync_destroy(&client_);
  }
}

Publisher::Publisher(Publisher &&other) noexcept
    : config_(std::move(other.config_)), client_(other.client_),
      seq_num_(other.seq_num_), bd_seq_num_(other.bd_seq_num_) {
  other.client_ = nullptr;
}

Publisher &Publisher::operator=(Publisher &&other) noexcept {
  if (this != &other) {
    if (client_) {
      MQTTAsync_destroy(&client_);
    }
    config_ = std::move(other.config_);
    client_ = other.client_;
    seq_num_ = other.seq_num_;
    bd_seq_num_ = other.bd_seq_num_;
    other.client_ = nullptr;
  }
  return *this;
}

std::expected<void, std::string> Publisher::connect() {
  int rc = MQTTAsync_create(&client_, config_.broker_url.c_str(),
                            config_.client_id.c_str(),
                            MQTTCLIENT_PERSISTENCE_NONE, nullptr);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to create client: {}", rc));
  }

  MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
  conn_opts.keepAliveInterval = 20;
  conn_opts.cleansession = config_.clean_session;

  MQTTAsync_willOptions will = MQTTAsync_willOptions_initializer;
  Topic death_topic{.group_id = config_.group_id,
                    .message_type = MessageType::NDEATH,
                    .edge_node_id = config_.edge_node_id,
                    .device_id = ""};
  auto death_topic_str = death_topic.to_string();
  will.topicName = death_topic_str.c_str();

  PayloadBuilder death_payload;
  death_payload.set_seq(seq_num_).add_metric("bdSeq", bd_seq_num_);
  auto death_data = death_payload.build();

  will.message = reinterpret_cast<const char *>(death_data.data());
  will.retained = 0;
  will.qos = config_.qos;

  conn_opts.will = &will;

  rc = MQTTAsync_connect(client_, &conn_opts);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to connect: {}", rc));
  }

  while (MQTTAsync_isConnected(client_) == 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  return {};
}

std::expected<void, std::string> Publisher::disconnect() {
  if (!client_) {
    return std::unexpected("Not connected");
  }

  MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
  int rc = MQTTAsync_disconnect(client_, &opts);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to disconnect: {}", rc));
  }

  return {};
}

std::expected<void, std::string>
Publisher::publish_birth(const PayloadBuilder &payload_builder) {
  if (!client_) {
    return std::unexpected("Not connected");
  }

  Topic topic{.group_id = config_.group_id,
              .message_type = MessageType::NBIRTH,
              .edge_node_id = config_.edge_node_id,
              .device_id = ""};

  auto topic_str = topic.to_string();
  auto payload_data = payload_builder.build();

  MQTTAsync_message msg = MQTTAsync_message_initializer;
  msg.payload = payload_data.data();
  msg.payloadlen = payload_data.size();
  msg.qos = config_.qos;
  msg.retained = 0;

  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;

  int rc = MQTTAsync_sendMessage(client_, topic_str.c_str(), &msg, &opts);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to publish: {}", rc));
  }

  seq_num_ = 0;
  bd_seq_num_++;

  return {};
}

std::expected<void, std::string>
Publisher::publish_data(const PayloadBuilder &payload_builder) {
  if (!client_) {
    return std::unexpected("Not connected");
  }

  Topic topic{.group_id = config_.group_id,
              .message_type = MessageType::NDATA,
              .edge_node_id = config_.edge_node_id,
              .device_id = ""};

  auto topic_str = topic.to_string();
  auto payload_data = payload_builder.build();

  MQTTAsync_message msg = MQTTAsync_message_initializer;
  msg.payload = payload_data.data();
  msg.payloadlen = payload_data.size();
  msg.qos = config_.qos;
  msg.retained = 0;

  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;

  int rc = MQTTAsync_sendMessage(client_, topic_str.c_str(), &msg, &opts);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to publish: {}", rc));
  }

  seq_num_ = (seq_num_ + 1) % 256;

  return {};
}

std::expected<void, std::string> Publisher::publish_death() {
  return disconnect();
}

} // namespace sparkplug