// src/subscriber.cpp
#include "sparkplug/subscriber.hpp"
#include <MQTTAsync.h>
#include <format>
#include <thread>

namespace sparkplug {

// Constructor - fix initialization order to match declaration order in class
Subscriber::Subscriber(Config config, MessageCallback callback)
    : callback_(std::move(callback)), config_(std::move(config)),
      client_(nullptr) {}

Subscriber::~Subscriber() {
  if (client_) {
    MQTTAsync_destroy(&client_);
  }
}

// Move constructor - fix initialization order
Subscriber::Subscriber(Subscriber &&other) noexcept
    : callback_(std::move(other.callback_)), config_(std::move(other.config_)),
      client_(other.client_) {
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
    other.client_ = nullptr;
  }
  return *this;
}

// Message arrived callback - must be static for C callback
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

  // Parse topic
  std::string topic_str(topicName, topicLen > 0 ? topicLen : strlen(topicName));
  auto topic_result = Topic::parse(topic_str);

  if (!topic_result) {
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
  }

  // Parse payload
  org::eclipse::tahu::protobuf::Payload payload;
  if (!payload.ParseFromArray(message->payload, message->payloadlen)) {
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
  }

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

// Connection lost callback
static void on_connection_lost(void *context, char *cause) {
  // Could add reconnection logic here
  (void)context;
  (void)cause;
}

std::expected<void, std::string> Subscriber::connect() {
  int rc = MQTTAsync_create(&client_, config_.broker_url.c_str(),
                            config_.client_id.c_str(),
                            MQTTCLIENT_PERSISTENCE_NONE, nullptr);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to create client: {}", rc));
  }

  // Set callbacks
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
  while (MQTTAsync_isConnected(client_) == 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  return {};
}

std::expected<void, std::string> Subscriber::disconnect() {
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

std::expected<void, std::string> Subscriber::subscribe_all() {
  if (!client_) {
    return std::unexpected("Not connected");
  }

  // Subscribe to all Sparkplug topics for this group
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

  // Subscribe to all message types for a specific edge node
  std::string topic =
      std::format("spBv1.0/{}/+/{}/#", config_.group_id, edge_node_id);

  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;

  int rc = MQTTAsync_subscribe(client_, topic.c_str(), config_.qos, &opts);
  if (rc != MQTTASYNC_SUCCESS) {
    return std::unexpected(std::format("Failed to subscribe: {}", rc));
  }

  return {};
}

} // namespace sparkplug