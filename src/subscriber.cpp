// src/subscriber.cpp
#include "sparkplug/subscriber.hpp"
#include <MQTTAsync.h>
#include <format>
#include <iostream>
#include <thread>

namespace sparkplug {

// Fix initialization order to match declaration
Subscriber::Subscriber(Config config, MessageCallback callback)
    : callback_(std::move(callback)), config_(std::move(config)),
      client_(nullptr) {}

Subscriber::~Subscriber() {
  if (client_) {
    MQTTAsync_destroy(&client_);
  }
}

Subscriber::Subscriber(Subscriber &&other) noexcept
    : callback_(std::move(other.callback_)), config_(std::move(other.config_)),
      client_(other.client_), node_states_(std::move(other.node_states_)) {
  other.client_ = nullptr;
}

Subscriber &Subscriber::operator=(Subscriber &&other) noexcept {
  if (this != &other) {
    if (client_) {
      MQTTAsync_destroy(&client_);
    }
    callback_ = std::move(other.callback_);
    config_ = std::move(other.config_);
    client_ = other.client_;
    node_states_ = std::move(other.node_states_);
    other.client_ = nullptr;
  }
  return *this;
}

bool Subscriber::validate_message(
    const Topic &topic, const org::eclipse::tahu::protobuf::Payload &payload) {
  if (!config_.validate_sequence) {
    return true;
  }

  const std::string &node_id = topic.edge_node_id;
  auto &state = node_states_[node_id];

  switch (topic.message_type) {
  case MessageType::NBIRTH: {
    // NBIRTH must have seq = 0
    if (payload.has_seq() && payload.seq() != 0) {
      std::cerr << "WARNING: NBIRTH for " << node_id
                << " has invalid seq: " << payload.seq() << " (expected 0)\n";
      return false;
    }

    // Extract bdSeq from metrics
    uint64_t bd_seq = 0;
    bool has_bdseq = false;
    for (const auto &metric : payload.metrics()) {
      if (metric.name() == "bdSeq") {
        bd_seq = metric.long_value();
        has_bdseq = true;
        break;
      }
    }

    if (!has_bdseq) {
      std::cerr << "WARNING: NBIRTH for " << node_id
                << " missing required bdSeq metric\n";
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
    // Extract bdSeq from metrics
    uint64_t bd_seq = 0;
    for (const auto &metric : payload.metrics()) {
      if (metric.name() == "bdSeq") {
        bd_seq = metric.long_value();
        break;
      }
    }

    // bdSeq in NDEATH should match NBIRTH
    if (state.birth_received && bd_seq != state.bd_seq) {
      std::cerr << "WARNING: NDEATH bdSeq mismatch for " << node_id
                << " (NDEATH: " << bd_seq << ", NBIRTH: " << state.bd_seq
                << ")\n";
    }

    state.is_online = false;
    return true;
  }

  case MessageType::NDATA:
  case MessageType::DDATA: {
    // Cannot receive DATA before BIRTH
    if (!state.birth_received) {
      std::cerr << "WARNING: Received "
                << (topic.message_type == MessageType::NDATA ? "NDATA"
                                                             : "DDATA")
                << " for " << node_id << " before NBIRTH\n";
      return false;
    }

    // Validate sequence number
    if (payload.has_seq()) {
      uint64_t seq = payload.seq();
      uint64_t expected_seq = (state.last_seq + 1) % 256;

      if (seq != expected_seq) {
        std::cerr << "WARNING: Sequence number gap for " << node_id << " (got "
                  << seq << ", expected " << expected_seq << ")\n";
        // Don't reject, just warn - could be packet loss
      }

      state.last_seq = seq;
    }

    return true;
  }

  case MessageType::DBIRTH: {
    // DBIRTH can have any seq > 0
    if (!state.birth_received) {
      std::cerr << "WARNING: Received DBIRTH for device on " << node_id
                << " before node NBIRTH\n";
      return false;
    }
    return true;
  }

  default:
    return true;
  }
}

void Subscriber::update_node_state(
    const Topic &topic, const org::eclipse::tahu::protobuf::Payload &payload) {
  // Update tracked state based on message
  validate_message(topic, payload);
}

static int on_message_arrived(void *context, char *topicName, int topicLen,
                              MQTTAsync_message *message) {
  auto *subscriber = static_cast<Subscriber *>(context);

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
    // STATE messages are plain UTF-8 strings
    std::string state_value(static_cast<char *>(message->payload),
                            message->payloadlen);

    // Create a dummy payload for STATE - not used by Sparkplug B encoding
    org::eclipse::tahu::protobuf::Payload dummy_payload;

    Topic state_topic{.group_id = "",
                      .message_type = MessageType::STATE,
                      .edge_node_id = topic_str.substr(6), // After "STATE/"
                      .device_id = ""};

    try {
      subscriber->callback_(state_topic, dummy_payload);
    } catch (...) {
      // Ignore exceptions from user code
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

  // Validate and update state
  subscriber->update_node_state(*topic_result, payload);

  // Call user callback
  try {
    subscriber->callback_(*topic_result, payload);
  } catch (...) {
    // Ignore exceptions from user code
  }

  MQTTAsync_freeMessage(&message);
  MQTTAsync_free(topicName);
  return 1;
}

static void on_connection_lost(void *context, char *cause) {
  (void)context;
  std::cerr << "Connection lost";
  if (cause) {
    std::cerr << ": " << cause;
  }
  std::cerr << "\n";
}

std::expected<void, std::string> Subscriber::connect() {
  int rc = MQTTAsync_create(&client_, config_.broker_url.c_str(),
                            config_.client_id.c_str(),
                            MQTTCLIENT_PERSISTENCE_NONE, nullptr);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to create client: {}", rc));
  }

  rc = MQTTAsync_setCallbacks(client_, this, on_connection_lost,
                              on_message_arrived, nullptr);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to set callbacks: {}", rc));
  }

  MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
  conn_opts.keepAliveInterval = 60;
  conn_opts.cleansession = config_.clean_session;

  rc = MQTTAsync_connect(client_, &conn_opts);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to connect: {}", rc));
  }

  // Wait for connection
  int timeout_ms = 5000;
  int elapsed_ms = 0;
  while (MQTTAsync_isConnected(client_) == 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    elapsed_ms += 100;
    if (elapsed_ms >= timeout_ms) {
      return std::unexpected("Connection timeout");
    }
  }

  return {};
}

std::expected<void, std::string> Subscriber::disconnect() {
  if (!client_) {
    return std::unexpected("Not connected");
  }

  MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
  opts.timeout = 10000;

  int rc = MQTTAsync_disconnect(client_, &opts);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to disconnect: {}", rc));
  }

  return {};
}

std::expected<void, std::string> Subscriber::subscribe_all() {
  if (!client_) {
    return std::unexpected("Not connected");
  }

  std::string topic = std::format("spBv1.0/{}/#", config_.group_id);

  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;

  int rc = MQTTAsync_subscribe(client_, topic.c_str(), config_.qos, &opts);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to subscribe: {}", rc));
  }

  return {};
}

std::expected<void, std::string>
Subscriber::subscribe_node(std::string_view edge_node_id) {
  if (!client_) {
    return std::unexpected("Not connected");
  }

  std::string topic =
      std::format("spBv1.0/{}/+/{}/#", config_.group_id, edge_node_id);

  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;

  int rc = MQTTAsync_subscribe(client_, topic.c_str(), config_.qos, &opts);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to subscribe: {}", rc));
  }

  return {};
}

std::expected<void, std::string>
Subscriber::subscribe_state(std::string_view host_id) {
  if (!client_) {
    return std::unexpected("Not connected");
  }

  std::string topic = std::format("STATE/{}", host_id);

  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;

  int rc = MQTTAsync_subscribe(client_, topic.c_str(), config_.qos, &opts);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to subscribe: {}", rc));
  }

  return {};
}

const Subscriber::NodeState *
Subscriber::get_node_state(const std::string &edge_node_id) const {
  auto it = node_states_.find(edge_node_id);
  if (it != node_states_.end()) {
    return &it->second;
  }
  return nullptr;
}

} // namespace sparkplug