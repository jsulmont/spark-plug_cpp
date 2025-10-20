// include/sparkplug/subscriber.hpp
#pragma once

#include "mqtt_handle.hpp"
#include "sparkplug_b.pb.h"
#include "topic.hpp"

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace sparkplug {

/**
 * @brief Callback function type for receiving Sparkplug B messages.
 *
 * @param topic Parsed Sparkplug B topic containing group_id, message_type, edge_node_id, etc.
 * @param payload Decoded Sparkplug B protobuf payload with metrics
 */
using MessageCallback =
    std::function<void(const Topic&, const org::eclipse::tahu::protobuf::Payload&)>;

/**
 * @brief Sparkplug B subscriber for consuming edge node messages with validation.
 *
 * The Subscriber class receives and validates Sparkplug B messages:
 * - NBIRTH/DBIRTH: Birth certificates
 * - NDATA/DDATA: Data updates
 * - NDEATH/DDEATH: Death certificates
 * - STATE: Primary application state (optional)
 * - Automatic sequence number validation (detects packet loss)
 * - Node state tracking per edge node
 *
 * @par Thread Safety
 * This class is NOT thread-safe. The callback is invoked on the MQTT thread.
 *
 * @par Example Usage
 * @code
 * auto callback = [](const sparkplug::Topic& topic,
 *                    const auto& payload) {
 *   std::cout << "Received: " << topic.to_string() << "\n";
 *   for (const auto& metric : payload.metrics()) {
 *     std::cout << metric.name() << " = " << metric.double_value() << "\n";
 *   }
 * };
 *
 * sparkplug::Subscriber::Config config{
 *   .broker_url = "tcp://localhost:1883",
 *   .client_id = "my_subscriber",
 *   .group_id = "Energy"
 * };
 *
 * sparkplug::Subscriber subscriber(std::move(config), callback);
 * subscriber.connect();
 * subscriber.subscribe_all();  // Subscribe to all messages in group
 *
 * // Keep running to receive messages...
 * @endcode
 *
 * @see Publisher for publishing Sparkplug B messages
 * @see Topic for topic namespace parsing
 */
class Subscriber {
public:
  /**
   * @brief Configuration parameters for the Sparkplug B subscriber.
   */
  struct Config {
    std::string broker_url;        ///< MQTT broker URL (e.g., "tcp://localhost:1883")
    std::string client_id;         ///< Unique MQTT client identifier
    std::string group_id;          ///< Sparkplug group ID to subscribe to
    int qos = 1;                   ///< MQTT QoS level (0, 1, or 2)
    bool clean_session = true;     ///< MQTT clean session flag
    bool validate_sequence = true; ///< Enable sequence number validation (detects packet loss)
  };

  /**
   * @brief Tracks the state of a device attached to an edge node.
   */
  struct DeviceState {
    bool is_online{false};      ///< True if DBIRTH received and device is online
    uint64_t last_seq{255};     ///< Last received device sequence number
    bool birth_received{false}; ///< True if DBIRTH has been received
  };

  /**
   * @brief Tracks the state of an individual edge node.
   *
   * Used internally for sequence validation and monitoring.
   */
  struct NodeState {
    bool is_online{false};       ///< True if NBIRTH received and node is online
    uint64_t last_seq{255};      ///< Last received node sequence number (starts at 255)
    uint64_t bd_seq{0};          ///< Current birth/death sequence number
    uint64_t birth_timestamp{0}; ///< Timestamp of last NBIRTH
    bool birth_received{false};  ///< True if NBIRTH has been received
    std::unordered_map<std::string, DeviceState> devices; ///< Attached devices (device_id -> state)
  };

  /**
   * @brief Constructs a Subscriber with the given configuration and callback.
   *
   * @param config Subscriber configuration (moved)
   * @param callback Function to call for each received message
   *
   * @note The callback will be invoked on the MQTT client thread.
   * @warning Ensure callback execution is fast to avoid blocking message reception.
   */
  Subscriber(Config config, MessageCallback callback);

  /**
   * @brief Destroys the Subscriber and cleans up MQTT resources.
   */
  ~Subscriber();

  Subscriber(const Subscriber&) = delete;
  Subscriber& operator=(const Subscriber&) = delete;
  Subscriber(Subscriber&&) noexcept;
  Subscriber& operator=(Subscriber&&) noexcept;

  /**
   * @brief Connects to the MQTT broker.
   *
   * @return void on success, error message on failure
   *
   * @note Must be called before subscribe_all() or subscribe_node().
   */
  [[nodiscard]] std::expected<void, std::string> connect();

  /**
   * @brief Disconnects from the MQTT broker.
   *
   * @return void on success, error message on failure
   *
   * @note Stops receiving messages after disconnect.
   */
  [[nodiscard]] std::expected<void, std::string> disconnect();

  /**
   * @brief Subscribes to all Sparkplug B messages for the configured group.
   *
   * Subscribes to the wildcard topic: spBv1.0/{group_id}/#
   *
   * This receives all message types (NBIRTH, NDATA, NDEATH, DBIRTH, DDATA, DDEATH)
   * from all edge nodes in the group.
   *
   * @return void on success, error message on failure
   *
   * @note Must call connect() first.
   * @note The callback will be invoked for every message received.
   */
  [[nodiscard]] std::expected<void, std::string> subscribe_all();

  /**
   * @brief Subscribes to messages from a specific edge node.
   *
   * Subscribes to: spBv1.0/{group_id}/+/{edge_node_id}/#
   *
   * @param edge_node_id The edge node ID to subscribe to
   *
   * @return void on success, error message on failure
   *
   * @note More efficient than subscribe_all() if you only need specific nodes.
   */
  [[nodiscard]] std::expected<void, std::string> subscribe_node(std::string_view edge_node_id);

  /**
   * @brief Subscribes to STATE messages from a primary application.
   *
   * STATE messages indicate whether a SCADA/Primary Application is online.
   * Subscribe to: STATE/{host_id}
   *
   * @param host_id The primary application host identifier
   *
   * @return void on success, error message on failure
   *
   * @note STATE messages are outside the normal Sparkplug topic namespace.
   */
  [[nodiscard]] std::expected<void, std::string> subscribe_state(std::string_view host_id);

  /**
   * @brief Gets the current state of a specific edge node.
   *
   * @param edge_node_id The edge node ID to query
   *
   * @return NodeState if the node has been seen, std::nullopt otherwise
   *
   * @note Useful for monitoring node online/offline status and bdSeq.
   */
  [[nodiscard]] std::optional<std::reference_wrapper<const NodeState>>
  get_node_state(std::string_view edge_node_id) const;

  /**
   * @brief Updates node state tracking (internal use).
   *
   * Called automatically from the MQTT callback to update sequence numbers
   * and online status.
   *
   * @param topic The message topic
   * @param payload The message payload
   *
   * @note This is public for technical reasons but should not be called directly.
   */
  void update_node_state(const Topic& topic, const org::eclipse::tahu::protobuf::Payload& payload);

  /**
   * @brief User-provided callback for received messages.
   *
   * @note Public for technical reasons (accessed by static MQTT callback).
   */
  MessageCallback callback_;

private:
  Config config_;
  MQTTAsyncHandle client_;

  // Track state of each edge node for validation
  std::unordered_map<std::string, NodeState> node_states_;

  bool validate_message(const Topic& topic, const org::eclipse::tahu::protobuf::Payload& payload);
};

} // namespace sparkplug