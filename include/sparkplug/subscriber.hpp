// include/sparkplug/subscriber.hpp
#pragma once

#include "sparkplug_b.pb.h"
#include "topic.hpp"
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

typedef void *MQTTAsync;

namespace sparkplug {

using MessageCallback = std::function<void(
    const Topic &, const org::eclipse::tahu::protobuf::Payload &)>;

class Subscriber {
public:
  struct Config {
    std::string broker_url;
    std::string client_id;
    std::string group_id;
    int qos = 1;
    bool clean_session = true;
    bool validate_sequence = true; // Enable sequence validation
  };

  struct NodeState {
    bool is_online{false};
    uint64_t last_seq{255}; // Start at 255 so first seq (0) is valid
    uint64_t bd_seq{0};
    uint64_t birth_timestamp{0};
    bool birth_received{false};
  };

  Subscriber(Config config, MessageCallback callback);
  ~Subscriber();

  Subscriber(const Subscriber &) = delete;
  Subscriber &operator=(const Subscriber &) = delete;
  Subscriber(Subscriber &&) noexcept;
  Subscriber &operator=(Subscriber &&) noexcept;

  [[nodiscard]] std::expected<void, std::string> connect();
  [[nodiscard]] std::expected<void, std::string> disconnect();

  // Subscribe to all messages for the configured group
  [[nodiscard]] std::expected<void, std::string> subscribe_all();

  // Subscribe to specific edge node
  [[nodiscard]] std::expected<void, std::string>
  subscribe_node(std::string_view edge_node_id);

  // Subscribe to STATE messages (for primary application monitoring)
  [[nodiscard]] std::expected<void, std::string>
  subscribe_state(std::string_view host_id);

  // Get node state for monitoring
  [[nodiscard]] const NodeState *
  get_node_state(const std::string &edge_node_id) const;

  // Internal: Update node state (called from MQTT callback)
  void update_node_state(const Topic &topic,
                         const org::eclipse::tahu::protobuf::Payload &payload);

  // Make callback_ accessible to static callback function
  MessageCallback callback_;

private:
  Config config_;
  MQTTAsync client_;

  // Track state of each edge node for validation
  std::unordered_map<std::string, NodeState> node_states_;

  bool validate_message(const Topic &topic,
                        const org::eclipse::tahu::protobuf::Payload &payload);
};

} // namespace sparkplug