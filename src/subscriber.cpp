// src/subscriber.cpp
#include "sparkplug/subscriber.hpp"

#include <format>
#include <future>
#include <iostream>
#include <thread>
#include <utility>

#include <MQTTAsync.h>

namespace sparkplug {

namespace {
// Timeout and connection constants
constexpr int CONNECTION_TIMEOUT_MS = 5000;
constexpr int DISCONNECT_TIMEOUT_MS = 10000;
constexpr uint64_t SEQ_NUMBER_MAX = 256;
constexpr int DEFAULT_KEEP_ALIVE_INTERVAL = 60;

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

Subscriber::Subscriber(Config config, MessageCallback callback)
    : callback_(std::move(callback)), config_(std::move(config)) {
}

Subscriber::~Subscriber() {
}

Subscriber::Subscriber(Subscriber&& other) noexcept
    : callback_(std::move(other.callback_)), config_(std::move(other.config_)),
      client_(std::move(other.client_)), node_states_(std::move(other.node_states_)) {
}

Subscriber& Subscriber::operator=(Subscriber&& other) noexcept {
  if (this != &other) {
    callback_ = std::move(other.callback_);
    config_ = std::move(other.config_);
    client_ = std::move(other.client_);
    node_states_ = std::move(other.node_states_);
  }
  return *this;
}

bool Subscriber::validate_message(const Topic& topic,
                                  const org::eclipse::tahu::protobuf::Payload& payload) {
  if (!config_.validate_sequence) {
    return true;
  }

  const std::string& node_id = topic.edge_node_id;
  auto& state = node_states_[node_id];

  switch (topic.message_type) {
  case MessageType::NBIRTH: {
    if (payload.has_seq() && payload.seq() != 0) {
      std::cerr << "WARNING: NBIRTH for " << node_id << " has invalid seq: " << payload.seq()
                << " (expected 0)\n";
      return false;
    }

    uint64_t bd_seq = 0;
    bool has_bdseq = false;
    for (const auto& metric : payload.metrics()) {
      if (metric.name() == "bdSeq") {
        bd_seq = metric.long_value();
        has_bdseq = true;
        break;
      }
    }

    if (!has_bdseq) {
      std::cerr << "WARNING: NBIRTH for " << node_id << " missing required bdSeq metric\n";
      return false;
    }

    state.bd_seq = bd_seq;
    state.last_seq = 0;
    state.is_online = true;
    state.birth_received = true;
    state.birth_timestamp = payload.timestamp();

    return true;
  }

  case MessageType::NDEATH: {
    uint64_t bd_seq = 0;
    for (const auto& metric : payload.metrics()) {
      if (metric.name() == "bdSeq") {
        bd_seq = metric.long_value();
        break;
      }
    }

    if (state.birth_received && bd_seq != state.bd_seq) {
      std::cerr << "WARNING: NDEATH bdSeq mismatch for " << node_id << " (NDEATH: " << bd_seq
                << ", NBIRTH: " << state.bd_seq << ")\n";
    }

    state.is_online = false;
    return true;
  }

  case MessageType::NDATA: {
    if (!state.birth_received) {
      std::cerr << "WARNING: Received NDATA for " << node_id << " before NBIRTH\n";
      return false;
    }

    if (payload.has_seq()) {
      uint64_t seq = payload.seq();
      uint64_t expected_seq = (state.last_seq + 1) % SEQ_NUMBER_MAX;

      if (seq != expected_seq) {
        std::cerr << "WARNING: Sequence number gap for " << node_id << " (got " << seq
                  << ", expected " << expected_seq << ")\n";
        // Don't reject, just warn - could be packet loss
      }

      state.last_seq = seq;
    }

    return true;
  }

  case MessageType::DBIRTH: {
    if (!state.birth_received) {
      std::cerr << "WARNING: Received DBIRTH for device on " << node_id << " before node NBIRTH\n";
      return false;
    }

    if (payload.has_seq() && payload.seq() != 0) {
      std::cerr << "WARNING: DBIRTH for device '" << topic.device_id << "' on " << node_id
                << " has invalid seq: " << payload.seq() << " (expected 0)\n";
    }

    auto& device_state = state.devices[topic.device_id];
    device_state.is_online = true;
    device_state.birth_received = true;
    device_state.last_seq = 0;

    return true;
  }

  case MessageType::DDATA: {
    if (!state.birth_received) {
      std::cerr << "WARNING: Received DDATA for device '" << topic.device_id << "' on " << node_id
                << " before node NBIRTH\n";
      return false;
    }

    auto device_it = state.devices.find(topic.device_id);
    if (device_it == state.devices.end() || !device_it->second.birth_received) {
      std::cerr << "WARNING: Received DDATA for device '" << topic.device_id << "' on " << node_id
                << " before DBIRTH\n";
      return false;
    }

    auto& device_state = device_it->second;

    if (payload.has_seq()) {
      uint64_t seq = payload.seq();
      uint64_t expected_seq = (device_state.last_seq + 1) % SEQ_NUMBER_MAX;

      if (seq != expected_seq) {
        std::cerr << "WARNING: Sequence number gap for device '" << topic.device_id << "' on "
                  << node_id << " (got " << seq << ", expected " << expected_seq << ")\n";
        // Don't reject, just warn - could be packet loss
      }

      device_state.last_seq = seq;
    }

    return true;
  }

  case MessageType::DDEATH: {
    auto device_it = state.devices.find(topic.device_id);
    if (device_it != state.devices.end()) {
      device_it->second.is_online = false;
    }
    return true;
  }

  case MessageType::NCMD:
  case MessageType::DCMD:
  case MessageType::STATE:
    return true;
  }
  std::unreachable();
}

void Subscriber::update_node_state(const Topic& topic,
                                   const org::eclipse::tahu::protobuf::Payload& payload) {
  validate_message(topic, payload);
}

void Subscriber::set_command_callback(CommandCallback callback) {
  command_callback_ = std::move(callback);
}

static int on_message_arrived(void* context, char* topicName, int topicLen,
                              MQTTAsync_message* message) {
  auto* subscriber = static_cast<Subscriber*>(context);

  if (!subscriber || !topicName || !message) {
    if (message) {
      MQTTAsync_freeMessage(&message);
      MQTTAsync_free(topicName);
    }
    return 1;
  }

  std::string topic_str(topicName, topicLen > 0 ? topicLen : strlen(topicName));

  // Handle STATE messages (plain text, not Sparkplug B)
  if (topic_str.starts_with("STATE/")) {
    std::string state_value(static_cast<char*>(message->payload), message->payloadlen);

    org::eclipse::tahu::protobuf::Payload dummy_payload;

    Topic state_topic{.group_id = "",
                      .message_type = MessageType::STATE,
                      .edge_node_id = topic_str.substr(6), // After "STATE/"
                      .device_id = ""};

    try {
      subscriber->callback_(state_topic, dummy_payload);
    } catch (...) {
    }

    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
  }

  auto topic_result = Topic::parse(topic_str);

  if (!topic_result) {
    std::cerr << "Failed to parse topic: " << topic_str << "\n";
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
  }

  org::eclipse::tahu::protobuf::Payload payload;
  if (!payload.ParseFromArray(message->payload, message->payloadlen)) {
    std::cerr << "Failed to parse Sparkplug B payload\n";
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
  }

  subscriber->update_node_state(*topic_result, payload);

  if ((topic_result->message_type == MessageType::NCMD ||
       topic_result->message_type == MessageType::DCMD) &&
      subscriber->command_callback_) {
    try {
      subscriber->command_callback_(*topic_result, payload);
    } catch (...) {
    }
  }

  try {
    subscriber->callback_(*topic_result, payload);
  } catch (...) {
  }

  MQTTAsync_freeMessage(&message);
  MQTTAsync_free(topicName);
  return 1;
}

static void on_connection_lost(void* context, char* cause) {
  (void)context;
  std::cerr << "Connection lost";
  if (cause) {
    std::cerr << ": " << cause;
  }
  std::cerr << "\n";
}

std::expected<void, std::string> Subscriber::connect() {
  MQTTAsync raw_client = nullptr;
  int rc = MQTTAsync_create(&raw_client, config_.broker_url.c_str(), config_.client_id.c_str(),
                            MQTTCLIENT_PERSISTENCE_NONE, nullptr);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to create client: {}", rc));
  }
  client_ = MQTTAsyncHandle(raw_client);

  rc = MQTTAsync_setCallbacks(client_.get(), this, on_connection_lost, on_message_arrived, nullptr);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to set callbacks: {}", rc));
  }

  std::promise<void> connect_promise;
  auto connect_future = connect_promise.get_future();

  MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
  conn_opts.keepAliveInterval = DEFAULT_KEEP_ALIVE_INTERVAL;
  conn_opts.cleansession = config_.clean_session;
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

  return {};
}

std::expected<void, std::string> Subscriber::disconnect() {
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
    return {};
  }

  try {
    disconnect_future.get();
  } catch (const std::exception&) {
  }

  return {};
}

std::expected<void, std::string> Subscriber::subscribe_all() {
  if (!client_) {
    return std::unexpected("Not connected");
  }

  std::string topic = std::format("spBv1.0/{}/#", config_.group_id);

  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;

  int rc = MQTTAsync_subscribe(client_.get(), topic.c_str(), config_.qos, &opts);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to subscribe: {}", rc));
  }

  return {};
}

std::expected<void, std::string> Subscriber::subscribe_node(std::string_view edge_node_id) {
  if (!client_) {
    return std::unexpected("Not connected");
  }

  std::string topic = std::format("spBv1.0/{}/+/{}/#", config_.group_id, edge_node_id);

  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;

  int rc = MQTTAsync_subscribe(client_.get(), topic.c_str(), config_.qos, &opts);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to subscribe: {}", rc));
  }

  return {};
}

std::expected<void, std::string> Subscriber::subscribe_state(std::string_view host_id) {
  if (!client_) {
    return std::unexpected("Not connected");
  }

  std::string topic = std::format("STATE/{}", host_id);

  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;

  int rc = MQTTAsync_subscribe(client_.get(), topic.c_str(), config_.qos, &opts);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to subscribe: {}", rc));
  }

  return {};
}

std::optional<std::reference_wrapper<const Subscriber::NodeState>>
Subscriber::get_node_state(std::string_view edge_node_id) const {
  auto it = node_states_.find(edge_node_id);
  if (it != node_states_.end()) {
    return std::cref(it->second);
  }
  return std::nullopt;
}

} // namespace sparkplug