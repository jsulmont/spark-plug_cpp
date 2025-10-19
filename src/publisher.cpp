// src/publisher.cpp
#include "sparkplug/publisher.hpp"

#include <cstring>
#include <format>
#include <thread>

#include <MQTTAsync.h>

namespace sparkplug {

namespace {
// Timeout and connection constants
constexpr int CONNECTION_TIMEOUT_MS = 5000;
constexpr int DISCONNECT_TIMEOUT_MS = 11000;
constexpr int POLL_INTERVAL_MS = 100;
constexpr uint64_t SEQ_NUMBER_MAX = 256;
} // namespace

MQTTAsyncHandle::~MQTTAsyncHandle() noexcept {
  reset();
}

void MQTTAsyncHandle::reset() noexcept {
  if (client_) {
    MQTTAsync_destroy(&client_);
    client_ = nullptr;
  }
}

Publisher::Publisher(Config config) : config_(std::move(config)) {
}

Publisher::~Publisher() {
  if (client_ && is_connected_) {
    (void)disconnect();
  }
  // unique_ptr will automatically call MQTTAsyncDeleter
}

Publisher::Publisher(Publisher&& other) noexcept
    : config_(std::move(other.config_)), client_(std::move(other.client_)),
      seq_num_(other.seq_num_), bd_seq_num_(other.bd_seq_num_),
      death_payload_data_(std::move(other.death_payload_data_)),
      last_birth_payload_(std::move(other.last_birth_payload_)),
      is_connected_(other.is_connected_) {
  other.is_connected_ = false;
}

Publisher& Publisher::operator=(Publisher&& other) noexcept {
  if (this != &other) {
    config_ = std::move(other.config_);
    client_ = std::move(other.client_);
    seq_num_ = other.seq_num_;
    bd_seq_num_ = other.bd_seq_num_;
    death_payload_data_ = std::move(other.death_payload_data_);
    last_birth_payload_ = std::move(other.last_birth_payload_);
    is_connected_ = other.is_connected_;
    other.is_connected_ = false;
  }
  return *this;
}

std::expected<void, std::string> Publisher::connect() {
  MQTTAsync raw_client = nullptr;
  int rc = MQTTAsync_create(&raw_client, config_.broker_url.c_str(), config_.client_id.c_str(),
                            MQTTCLIENT_PERSISTENCE_NONE, nullptr);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to create client: {}", rc));
  }
  client_ = MQTTAsyncHandle(raw_client);

  // Prepare NDEATH payload BEFORE connecting
  PayloadBuilder death_payload;
  death_payload.add_metric("bdSeq", bd_seq_num_);
  death_payload_data_ = death_payload.build();

  // Setup connection options
  MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
  conn_opts.keepAliveInterval = config_.keep_alive_interval;
  conn_opts.cleansession = config_.clean_session;

  // Setup Last Will and Testament (NDEATH)
  MQTTAsync_willOptions will = MQTTAsync_willOptions_initializer;

  Topic death_topic{.group_id = config_.group_id,
                    .message_type = MessageType::NDEATH,
                    .edge_node_id = config_.edge_node_id,
                    .device_id = ""};

  auto death_topic_str = death_topic.to_string();
  will.topicName = death_topic_str.c_str();

  // CRITICAL FIX: Use payload.data and payload.len for binary data
  // NOT will.message which expects null-terminated string!
  will.payload.data = death_payload_data_.data();
  will.payload.len = static_cast<int>(death_payload_data_.size());
  will.retained = 0;
  will.qos = config_.qos;

  conn_opts.will = &will;

  // Connect to broker
  rc = MQTTAsync_connect(client_.get(), &conn_opts);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to connect: {}", rc));
  }

  // Wait for connection with timeout
  int elapsed_ms = 0;
  while (MQTTAsync_isConnected(client_.get()) == 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
    elapsed_ms += POLL_INTERVAL_MS;
    if (elapsed_ms >= CONNECTION_TIMEOUT_MS) {
      return std::unexpected("Connection timeout");
    }
  }

  is_connected_ = true;
  return {};
}

std::expected<void, std::string> Publisher::disconnect() {
  if (!client_) {
    return std::unexpected("Not connected");
  }

  MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;

  int rc = MQTTAsync_disconnect(client_.get(), &opts);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to disconnect: {}", rc));
  }

  // Wait for disconnect
  int elapsed_ms = 0;
  while (MQTTAsync_isConnected(client_.get()) != 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
    elapsed_ms += POLL_INTERVAL_MS;
    if (elapsed_ms >= DISCONNECT_TIMEOUT_MS) {
      break;
    }
  }

  is_connected_ = false;
  return {};
}

std::expected<void, std::string> Publisher::publish_message(const Topic& topic,
                                                            std::span<const uint8_t> payload_data) {
  if (!client_ || !is_connected_) {
    return std::unexpected("Not connected");
  }

  auto topic_str = topic.to_string();

  MQTTAsync_message msg = MQTTAsync_message_initializer;
  msg.payload = const_cast<void*>(reinterpret_cast<const void*>(payload_data.data()));
  msg.payloadlen = static_cast<int>(payload_data.size());
  msg.qos = config_.qos;
  msg.retained = 0;

  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;

  int rc = MQTTAsync_sendMessage(client_.get(), topic_str.c_str(), &msg, &opts);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to publish: {}", rc));
  }

  return {};
}

std::expected<void, std::string> Publisher::publish_birth(PayloadBuilder& payload) {
  if (!is_connected_) {
    return std::unexpected("Not connected");
  }

  payload.set_seq(0);

  bool has_bdseq = false;
  auto& proto_payload = payload.mutable_payload();

  for (const auto& metric : proto_payload.metrics()) {
    if (metric.name() == "bdSeq") {
      has_bdseq = true;
      break;
    }
  }

  if (!has_bdseq) {
    auto* metric = proto_payload.add_metrics();
    metric->set_name("bdSeq");
    metric->set_datatype(static_cast<uint32_t>(DataType::UInt64));
    metric->set_long_value(bd_seq_num_);
  }

  Topic topic{.group_id = config_.group_id,
              .message_type = MessageType::NBIRTH,
              .edge_node_id = config_.edge_node_id,
              .device_id = ""};

  auto payload_data = payload.build();
  last_birth_payload_ = payload_data;

  auto result = publish_message(topic, payload_data);
  if (!result) {
    return result;
  }

  seq_num_ = 0;
  bd_seq_num_++;

  return {};
}

std::expected<void, std::string> Publisher::publish_data(PayloadBuilder& payload) {
  if (!is_connected_) {
    return std::unexpected("Not connected");
  }

  seq_num_ = (seq_num_ + 1) % SEQ_NUMBER_MAX;

  if (!payload.has_seq()) {
    payload.set_seq(seq_num_);
  }

  Topic topic{.group_id = config_.group_id,
              .message_type = MessageType::NDATA,
              .edge_node_id = config_.edge_node_id,
              .device_id = ""};

  auto payload_data = payload.build();
  return publish_message(topic, payload_data);
}

std::expected<void, std::string> Publisher::publish_death() {
  if (!is_connected_) {
    return std::unexpected("Not connected");
  }

  Topic topic{.group_id = config_.group_id,
              .message_type = MessageType::NDEATH,
              .edge_node_id = config_.edge_node_id,
              .device_id = ""};

  auto result = publish_message(topic, death_payload_data_);
  if (!result) {
    return result;
  }

  return disconnect();
}

std::expected<void, std::string> Publisher::rebirth() {
  if (!is_connected_) {
    return std::unexpected("Not connected");
  }

  if (last_birth_payload_.empty()) {
    return std::unexpected("No previous birth payload stored");
  }

  bd_seq_num_++;

  org::eclipse::tahu::protobuf::Payload proto_payload;
  if (!proto_payload.ParseFromArray(last_birth_payload_.data(),
                                    static_cast<int>(last_birth_payload_.size()))) {
    return std::unexpected("Failed to parse stored birth payload");
  }

  for (auto& metric : *proto_payload.mutable_metrics()) {
    if (metric.name() == "bdSeq") {
      metric.set_long_value(bd_seq_num_);
      break;
    }
  }

  proto_payload.set_seq(0);

  Topic topic{.group_id = config_.group_id,
              .message_type = MessageType::NBIRTH,
              .edge_node_id = config_.edge_node_id,
              .device_id = ""};

  std::vector<uint8_t> payload_data(proto_payload.ByteSizeLong());
  proto_payload.SerializeToArray(payload_data.data(), static_cast<int>(payload_data.size()));

  auto result = publish_message(topic, payload_data);
  if (!result) {
    return result;
  }

  last_birth_payload_ = payload_data;
  seq_num_ = 0;

  return {};
}

} // namespace sparkplug