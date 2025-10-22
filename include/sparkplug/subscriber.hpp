// include/sparkplug/subscriber.hpp
#pragma once

#include "mqtt_handle.hpp"
#include "sparkplug_b.pb.h"
#include "topic.hpp"

#include <expected>
#include <functional>
#include <memory>
#include <mutex>
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
 * @brief Callback function type for receiving Sparkplug B command messages (NCMD/DCMD).
 *
 * Commands are special messages sent from SCADA/Primary Applications to edge nodes
 * to trigger actions like rebirth, reboot, or custom operations.
 *
 * @param topic Parsed command topic (message_type will be NCMD or DCMD)
 * @param payload Command payload containing metrics with command names and values
 *
 * @note Common Node Control commands:
 *       - "Node Control/Rebirth" (bool): Request node to republish NBIRTH
 *       - "Node Control/Reboot" (bool): Request node to reboot
 *       - "Node Control/Next Server" (bool): Switch to backup server
 *       - "Node Control/Scan Rate" (int64): Change data acquisition rate
 */
using CommandCallback =
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
 * This class is thread-safe. All methods may be called from any thread concurrently.
 * Internal synchronization is handled via mutex locking.
 * Note: Callbacks are invoked on the MQTT client thread.
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
   * @brief TLS/SSL configuration options for secure MQTT connections.
   */
  struct TlsOptions {
    std::string trust_store;             ///< Path to CA certificate file (PEM format)
    std::string key_store;               ///< Path to client certificate file (PEM format, optional)
    std::string private_key;             ///< Path to client private key file (PEM format, optional)
    std::string private_key_password;    ///< Password for encrypted private key (optional)
    std::string enabled_cipher_suites;   ///< Colon-separated list of cipher suites (optional)
    bool enable_server_cert_auth = true; ///< Verify server certificate (default: true)
  };

  /**
   * @brief Configuration parameters for the Sparkplug B subscriber.
   */
  struct Config {
    std::string
        broker_url; ///< MQTT broker URL (e.g., "tcp://localhost:1883" or "ssl://localhost:8883")
    std::string client_id;           ///< Unique MQTT client identifier
    std::string group_id;            ///< Sparkplug group ID to subscribe to
    int qos = 1;                     ///< MQTT QoS level (0, 1, or 2)
    bool clean_session = true;       ///< MQTT clean session flag
    bool validate_sequence = true;   ///< Enable sequence number validation (detects packet loss)
    std::optional<TlsOptions> tls{}; ///< TLS/SSL options (required if broker_url uses ssl://)
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
  ~Subscriber() = default;

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
   * @brief Subscribes to all messages for an additional group.
   *
   * Subscribes to: spBv1.0/{group_id}/#
   *
   * @param group_id The group ID to subscribe to
   *
   * @return void on success, error message on failure
   *
   * @note Allows subscribing to multiple groups on a single MQTT connection.
   */
  [[nodiscard]] std::expected<void, std::string> subscribe_group(std::string_view group_id);

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
   * @param group_id The group ID
   * @param edge_node_id The edge node ID to query
   *
   * @return NodeState if the node has been seen, std::nullopt otherwise
   *
   * @note Useful for monitoring node online/offline status and bdSeq.
   */
  [[nodiscard]] std::optional<std::reference_wrapper<const NodeState>>
  get_node_state(std::string_view group_id, std::string_view edge_node_id) const;

  /**
   * @brief Sets a callback for receiving command messages (NCMD/DCMD).
   *
   * Commands are messages sent from SCADA/Primary Applications to edge nodes
   * to trigger actions like rebirth, reboot, or custom operations.
   *
   * @param callback Function to call when a command is received
   *
   * @note The callback is invoked on the MQTT client thread.
   * @note Common commands: "Node Control/Rebirth", "Node Control/Reboot", etc.
   *
   * @par Example Usage
   * @code
   * subscriber.set_command_callback([&publisher](const sparkplug::Topic& topic,
   *                                              const auto& payload) {
   *   for (const auto& metric : payload.metrics()) {
   *     if (metric.name() == "Node Control/Rebirth" && metric.boolean_value()) {
   *       publisher.rebirth();
   *     }
   *   }
   * });
   * @endcode
   */
  void set_command_callback(CommandCallback callback);

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

  /**
   * @brief User-provided callback for received command messages (NCMD/DCMD).
   *
   * @note Public for technical reasons (accessed by static MQTT callback).
   */
  CommandCallback command_callback_;

private:
  Config config_;
  MQTTAsyncHandle client_;

  struct NodeKey {
    std::string group_id;
    std::string edge_node_id;

    bool operator==(const NodeKey& other) const noexcept {
      return group_id == other.group_id && edge_node_id == other.edge_node_id;
    }
  };

  struct NodeKeyHash {
    using is_transparent = void;
    [[nodiscard]] size_t operator()(const NodeKey& key) const noexcept {
      size_t h1 = std::hash<std::string>{}(key.group_id);
      size_t h2 = std::hash<std::string>{}(key.edge_node_id);
      return h1 ^ (h2 << 1);
    }
    [[nodiscard]] size_t
    operator()(std::pair<std::string_view, std::string_view> key) const noexcept {
      size_t h1 = std::hash<std::string_view>{}(key.first);
      size_t h2 = std::hash<std::string_view>{}(key.second);
      return h1 ^ (h2 << 1);
    }
  };

  struct NodeKeyEqual {
    using is_transparent = void;
    [[nodiscard]] bool operator()(const NodeKey& lhs, const NodeKey& rhs) const noexcept {
      return lhs == rhs;
    }
    [[nodiscard]] bool operator()(const NodeKey& lhs,
                                  std::pair<std::string_view, std::string_view> rhs) const noexcept {
      return lhs.group_id == rhs.first && lhs.edge_node_id == rhs.second;
    }
    [[nodiscard]] bool operator()(std::pair<std::string_view, std::string_view> lhs,
                                  const NodeKey& rhs) const noexcept {
      return lhs.first == rhs.group_id && lhs.second == rhs.edge_node_id;
    }
  };

  std::unordered_map<NodeKey, NodeState, NodeKeyHash, NodeKeyEqual> node_states_;

  mutable std::mutex mutex_;

  bool validate_message(const Topic& topic, const org::eclipse::tahu::protobuf::Payload& payload);
};

} // namespace sparkplug