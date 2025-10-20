// src/publisher.cpp
#include "sparkplug/publisher.hpp"

#include <cstring>
#include <format>
#include <future>
#include <thread>

#include <MQTTAsync.h>

namespace sparkplug {

namespace {
// Timeout and connection constants
constexpr int CONNECTION_TIMEOUT_MS = 5000;
constexpr int DISCONNECT_TIMEOUT_MS = 11000;
constexpr uint64_t SEQ_NUMBER_MAX = 256;

// Callback for successful connection
void on_connect_success(void* context, MQTTAsync_successData* response) {
  (void)response;
  auto* promise = static_cast<std::promise<void>*>(context);
  promise->set_value();
}

// Callback for failed connection
void on_connect_failure(void* context, MQTTAsync_failureData* response) {
  auto* promise = static_cast<std::promise<void>*>(context);
  auto error = std::format("Connection failed: code={}", response ? response->code : -1);
  promise->set_exception(std::make_exception_ptr(std::runtime_error(error)));
}

// Callback for successful disconnection
void on_disconnect_success(void* context, MQTTAsync_successData* response) {
  (void)response;
  auto* promise = static_cast<std::promise<void>*>(context);
  promise->set_value();
}

// Callback for failed disconnection
void on_disconnect_failure(void* context, MQTTAsync_failureData* response) {
  auto* promise = static_cast<std::promise<void>*>(context);
  auto error = std::format("Disconnect failed: code={}", response ? response->code : -1);
  promise->set_exception(std::make_exception_ptr(std::runtime_error(error)));
}

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

  std::promise<void> connect_promise;
  auto connect_future = connect_promise.get_future();

  conn_opts.context = &connect_promise;
  conn_opts.onSuccess = on_connect_success;
  conn_opts.onFailure = on_connect_failure;

  rc = MQTTAsync_connect(client_.get(), &conn_opts);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to connect: {}", rc));
  }

  auto status = connect_future.wait_for(std::chrono::milliseconds(CONNECTION_TIMEOUT_MS));
  if (status == std::future_status::timeout) {
    return std::unexpected("Connection timeout");
  }

  try {
    connect_future.get();
  } catch (const std::exception& e) {
    return std::unexpected(e.what());
  }

  is_connected_ = true;
  return {};
}

std::expected<void, std::string> Publisher::disconnect() {
  if (!client_) {
    return std::unexpected("Not connected");
  }

  std::promise<void> disconnect_promise;
  auto disconnect_future = disconnect_promise.get_future();

  MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
  opts.timeout = DISCONNECT_TIMEOUT_MS;
  opts.context = &disconnect_promise;
  opts.onSuccess = on_disconnect_success;
  opts.onFailure = on_disconnect_failure;

  int rc = MQTTAsync_disconnect(client_.get(), &opts);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to disconnect: {}", rc));
  }

  auto status = disconnect_future.wait_for(std::chrono::milliseconds(DISCONNECT_TIMEOUT_MS));
  if (status == std::future_status::timeout) {
    is_connected_ = false;
    return {};
  }

  try {
    disconnect_future.get();
  } catch (const std::exception&) {
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

  org::eclipse::tahu::protobuf::Payload proto_payload;
  if (!proto_payload.ParseFromArray(last_birth_payload_.data(),
                                    static_cast<int>(last_birth_payload_.size()))) {
    return std::unexpected("Failed to parse stored birth payload");
  }

  bd_seq_num_++;

  for (auto& metric : *proto_payload.mutable_metrics()) {
    if (metric.name() == "bdSeq") {
      metric.set_long_value(bd_seq_num_);
      break;
    }
  }

  proto_payload.set_seq(0);

  std::vector<uint8_t> payload_data(proto_payload.ByteSizeLong());
  proto_payload.SerializeToArray(payload_data.data(), static_cast<int>(payload_data.size()));
  last_birth_payload_ = payload_data;

  // Disconnect (sends old NDEATH), then reconnect (sets new NDEATH with new bdSeq)
  auto disc_result = disconnect();
  if (!disc_result) {
    return disc_result;
  }

  auto conn_result = connect();
  if (!conn_result) {
    return conn_result;
  }

  Topic topic{.group_id = config_.group_id,
              .message_type = MessageType::NBIRTH,
              .edge_node_id = config_.edge_node_id,
              .device_id = ""};

  auto result = publish_message(topic, payload_data);
  if (!result) {
    return result;
  }

  seq_num_ = 0;

  return {};
}

std::expected<void, std::string> Publisher::publish_device_birth(std::string_view device_id,
                                                                 PayloadBuilder& payload) {
  if (!is_connected_) {
    return std::unexpected("Not connected");
  }

  if (last_birth_payload_.empty()) {
    return std::unexpected("Must publish NBIRTH before DBIRTH");
  }

  payload.set_seq(0);

  Topic topic{.group_id = config_.group_id,
              .message_type = MessageType::DBIRTH,
              .edge_node_id = config_.edge_node_id,
              .device_id = std::string(device_id)};

  auto payload_data = payload.build();

  auto result = publish_message(topic, payload_data);
  if (!result) {
    return result;
  }

  auto& device_state = device_states_[std::string(device_id)];
  device_state.seq_num = 0;
  device_state.last_birth_payload = payload_data;
  device_state.is_online = true;

  return {};
}

std::expected<void, std::string> Publisher::publish_device_data(std::string_view device_id,
                                                                PayloadBuilder& payload) {
  if (!is_connected_) {
    return std::unexpected("Not connected");
  }

  auto it = device_states_.find(device_id);
  if (it == device_states_.end() || !it->second.is_online) {
    return std::unexpected(
        std::format("Must publish DBIRTH for device '{}' before DDATA", device_id));
  }

  auto& device_state = it->second;
  device_state.seq_num = (device_state.seq_num + 1) % SEQ_NUMBER_MAX;

  if (!payload.has_seq()) {
    payload.set_seq(device_state.seq_num);
  }

  Topic topic{.group_id = config_.group_id,
              .message_type = MessageType::DDATA,
              .edge_node_id = config_.edge_node_id,
              .device_id = std::string(device_id)};

  auto payload_data = payload.build();
  return publish_message(topic, payload_data);
}

std::expected<void, std::string> Publisher::publish_device_death(std::string_view device_id) {
  if (!is_connected_) {
    return std::unexpected("Not connected");
  }

  auto it = device_states_.find(device_id);
  if (it == device_states_.end()) {
    return std::unexpected(std::format("Unknown device: '{}'", device_id));
  }

  PayloadBuilder death_payload;

  Topic topic{.group_id = config_.group_id,
              .message_type = MessageType::DDEATH,
              .edge_node_id = config_.edge_node_id,
              .device_id = std::string(device_id)};

  auto payload_data = death_payload.build();
  auto result = publish_message(topic, payload_data);
  if (!result) {
    return result;
  }

  it->second.is_online = false;
  return {};
}

std::expected<void, std::string>
Publisher::publish_node_command(std::string_view target_edge_node_id, PayloadBuilder& payload) {
  if (!is_connected_) {
    return std::unexpected("Not connected");
  }

  Topic topic{.group_id = config_.group_id,
              .message_type = MessageType::NCMD,
              .edge_node_id = std::string(target_edge_node_id),
              .device_id = ""};

  auto payload_data = payload.build();
  return publish_message(topic, payload_data);
}

std::expected<void, std::string>
Publisher::publish_device_command(std::string_view target_edge_node_id,
                                  std::string_view target_device_id, PayloadBuilder& payload) {
  if (!is_connected_) {
    return std::unexpected("Not connected");
  }

  Topic topic{.group_id = config_.group_id,
              .message_type = MessageType::DCMD,
              .edge_node_id = std::string(target_edge_node_id),
              .device_id = std::string(target_device_id)};

  auto payload_data = payload.build();
  return publish_message(topic, payload_data);
}

} // namespace sparkplug