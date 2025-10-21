// include/sparkplug/publisher.hpp
#pragma once

#include "mqtt_handle.hpp"
#include "payload_builder.hpp"
#include "topic.hpp"

#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace sparkplug {

/**
 * @brief Sparkplug B edge node publisher implementing the complete message lifecycle.
 *
 * The Publisher class manages the full Sparkplug B protocol for an edge node:
 * - NBIRTH: Initial birth certificate with all metrics and aliases
 * - NDATA: Subsequent data updates using aliases for bandwidth efficiency
 * - NDEATH: Death certificate (sent via MQTT Last Will Testament)
 * - Automatic sequence number management (0-255, wraps at 256)
 * - Birth/Death sequence (bdSeq) tracking for session management
 *
 * @par Thread Safety
 * This class is thread-safe. All methods may be called from any thread concurrently.
 * Internal synchronization is handled via mutex locking.
 *
 * @par Example Usage
 * @code
 * sparkplug::Publisher::Config config{
 *   .broker_url = "tcp://localhost:1883",
 *   .client_id = "my_edge_node",
 *   .group_id = "Energy",
 *   .edge_node_id = "Gateway01"
 * };
 *
 * sparkplug::Publisher publisher(std::move(config));
 * publisher.connect();
 *
 * // Publish NBIRTH (required first message)
 * sparkplug::PayloadBuilder birth;
 * birth.add_metric_with_alias("Temperature", 1, 20.5);
 * publisher.publish_birth(birth);
 *
 * // Publish NDATA updates
 * sparkplug::PayloadBuilder data;
 * data.add_metric_by_alias(1, 21.0);  // Temperature changed
 * publisher.publish_data(data);
 *
 * publisher.disconnect();  // Sends NDEATH automatically
 * @endcode
 *
 * @see PayloadBuilder for constructing metric payloads
 * @see Subscriber for consuming Sparkplug B messages
 */
class Publisher {
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
   * @brief Configuration parameters for the Sparkplug B publisher.
   */
  struct Config {
    std::string
        broker_url; ///< MQTT broker URL (e.g., "tcp://localhost:1883" or "ssl://localhost:8883")
    std::string client_id;        ///< Unique MQTT client identifier
    std::string group_id;         ///< Sparkplug group ID (topic namespace)
    std::string edge_node_id;     ///< Edge node identifier within the group
    int qos = 1;                  ///< MQTT QoS level (0, 1, or 2). Sparkplug recommends 1.
    bool clean_session = true;    ///< MQTT clean session flag
    int keep_alive_interval = 60; ///< MQTT keep-alive interval in seconds (Sparkplug recommends 60)
    std::optional<TlsOptions> tls{}; ///< TLS/SSL options (required if broker_url uses ssl://)
  };

  /**
   * @brief Constructs a Publisher with the given configuration.
   *
   * @param config Publisher configuration (moved)
   *
   * @note The NDEATH payload is prepared during construction and will be
   *       sent automatically when the MQTT connection is lost.
   */
  Publisher(Config config);

  /**
   * @brief Destroys the Publisher and cleans up MQTT resources.
   */
  ~Publisher();

  Publisher(const Publisher&) = delete;
  Publisher& operator=(const Publisher&) = delete;
  Publisher(Publisher&&) noexcept;
  Publisher& operator=(Publisher&&) noexcept;

  /**
   * @brief Connects to the MQTT broker and establishes a Sparkplug B session.
   *
   * Sets the NDEATH message as the MQTT Last Will Testament before connecting.
   * The NDEATH will be sent automatically if the connection is lost unexpectedly.
   *
   * @return void on success, error message on failure
   *
   * @note Must be called before publish_birth().
   * @warning The Publisher must remain in scope while connected, or NDEATH
   *          may not be delivered properly.
   */
  [[nodiscard]] std::expected<void, std::string> connect();

  /**
   * @brief Gracefully disconnects from the MQTT broker.
   *
   * Sends NDEATH via MQTT Last Will Testament and closes the connection.
   *
   * @return void on success, error message on failure
   *
   * @note After disconnect, you can call connect() again to reconnect.
   */
  [[nodiscard]] std::expected<void, std::string> disconnect();

  /**
   * @brief Publishes an NBIRTH (Node Birth) message.
   *
   * The NBIRTH message must be the first message published after connect().
   * It establishes the session and declares all available metrics with their aliases.
   *
   * @param payload PayloadBuilder containing metrics with names and aliases
   *
   * @return void on success, error message on failure
   *
   * @note The payload should include:
   *       - All metrics with both name and alias (for NDATA to use aliases)
   *       - bdSeq metric (automatically managed if using rebirth())
   *       - Any metadata or properties
   *
   * @warning Must be called after connect() and before any publish_data() calls.
   *
   * @see publish_data() for subsequent updates
   * @see rebirth() for publishing a new NBIRTH during runtime
   */
  [[nodiscard]] std::expected<void, std::string> publish_birth(PayloadBuilder& payload);

  /**
   * @brief Publishes an NDATA (Node Data) message.
   *
   * NDATA messages report metric changes by exception. Only include metrics
   * that have changed since the last NDATA message. Uses aliases for bandwidth
   * efficiency (60-80% reduction vs. full names).
   *
   * @param payload PayloadBuilder containing changed metrics (by alias only)
   *
   * @return void on success, error message on failure
   *
   * @note Sequence number is automatically incremented (0-255, wraps at 256).
   * @note Timestamp is automatically added if not explicitly set.
   *
   * @warning Must call publish_birth() before the first publish_data().
   *
   * @see publish_birth() for establishing aliases
   */
  [[nodiscard]] std::expected<void, std::string> publish_data(PayloadBuilder& payload);

  /**
   * @brief Publishes an NDEATH (Node Death) message.
   *
   * Explicitly sends the NDEATH message. Usually not needed as NDEATH is
   * sent automatically via MQTT Last Will Testament on disconnect or connection loss.
   *
   * @return void on success, error message on failure
   *
   * @note Prefer using disconnect() which handles NDEATH automatically.
   */
  [[nodiscard]] std::expected<void, std::string> publish_death();

  /**
   * @brief Triggers a rebirth by publishing a new NBIRTH with incremented bdSeq.
   *
   * Rebirth is used when:
   * - SCADA/Primary Application requests it via NCMD/Rebirth
   * - New metrics need to be added to the metric inventory
   * - Edge node configuration changes
   *
   * @return void on success, error message on failure
   *
   * @note Automatically increments bdSeq and resets sequence number to 0.
   * @note Republishes the last NBIRTH payload with updated bdSeq.
   *
   * @warning The new NBIRTH should contain ALL metrics (old + new), not just additions.
   */
  [[nodiscard]] std::expected<void, std::string> rebirth();

  /**
   * @brief Gets the current message sequence number.
   *
   * @return Current sequence number (0-255, wraps at 256)
   *
   * @note Useful for monitoring and debugging.
   */
  [[nodiscard]] uint64_t get_seq() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return seq_num_;
  }

  /**
   * @brief Gets the current birth/death sequence number.
   *
   * @return Current bdSeq value (increments on each rebirth, never wraps)
   *
   * @note Used by SCADA to detect new sessions/rebirths.
   */
  [[nodiscard]] uint64_t get_bd_seq() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bd_seq_num_;
  }

  /**
   * @brief Publishes a DBIRTH (Device Birth) message.
   *
   * The DBIRTH message declares a device attached to this edge node.
   * It must be published after NBIRTH and declares all device metrics with aliases.
   *
   * @param device_id The device identifier (e.g., "Sensor01", "Motor02")
   * @param payload PayloadBuilder containing device metrics with names and aliases
   *
   * @return void on success, error message on failure
   *
   * @note Device sequence starts at 0 for DBIRTH, then increments for DDATA.
   * @note Must call publish_birth() before publishing any device births.
   *
   * @see publish_device_data() for subsequent device updates
   * @see publish_device_death() for device disconnection
   */
  [[nodiscard]] std::expected<void, std::string> publish_device_birth(std::string_view device_id,
                                                                      PayloadBuilder& payload);

  /**
   * @brief Publishes a DDATA (Device Data) message.
   *
   * DDATA messages report device metric changes by exception. Only include metrics
   * that have changed since the last DDATA message. Uses aliases for bandwidth efficiency.
   *
   * @param device_id The device identifier
   * @param payload PayloadBuilder containing changed metrics (by alias only)
   *
   * @return void on success, error message on failure
   *
   * @note Sequence number is automatically incremented per device (0-255, wraps at 256).
   * @note Must call publish_device_birth() before the first publish_device_data().
   *
   * @see publish_device_birth() for establishing aliases
   */
  [[nodiscard]] std::expected<void, std::string> publish_device_data(std::string_view device_id,
                                                                     PayloadBuilder& payload);

  /**
   * @brief Publishes a DDEATH (Device Death) message.
   *
   * Explicitly sends a device death message to indicate device disconnection.
   *
   * @param device_id The device identifier
   *
   * @return void on success, error message on failure
   *
   * @note After DDEATH, publish_device_birth() must be called again before DDATA.
   */
  [[nodiscard]] std::expected<void, std::string> publish_device_death(std::string_view device_id);

  /**
   * @brief Publishes an NCMD (Node Command) message to another edge node.
   *
   * NCMD messages are commands sent from SCADA/Primary Applications or other edge nodes
   * to request actions like rebirth, reboot, or custom operations.
   *
   * @param target_edge_node_id The target edge node identifier
   * @param payload PayloadBuilder containing command metrics (e.g., "Node Control/Rebirth")
   *
   * @return void on success, error message on failure
   *
   * @note Common Node Control commands:
   *       - "Node Control/Rebirth" (bool): Request node to republish NBIRTH
   *       - "Node Control/Reboot" (bool): Request node to reboot
   *       - "Node Control/Next Server" (bool): Switch to backup server
   *       - "Node Control/Scan Rate" (int64): Change data acquisition rate
   *
   * @par Example Usage
   * @code
   * sparkplug::PayloadBuilder cmd;
   * cmd.add_metric("Node Control/Rebirth", true);
   * publisher.publish_node_command("Gateway01", cmd);
   * @endcode
   */
  [[nodiscard]] std::expected<void, std::string>
  publish_node_command(std::string_view target_edge_node_id, PayloadBuilder& payload);

  /**
   * @brief Publishes a DCMD (Device Command) message to a device on another edge node.
   *
   * DCMD messages are commands sent to devices attached to edge nodes.
   *
   * @param target_edge_node_id The target edge node identifier
   * @param target_device_id The target device identifier
   * @param payload PayloadBuilder containing command metrics
   *
   * @return void on success, error message on failure
   *
   * @par Example Usage
   * @code
   * sparkplug::PayloadBuilder cmd;
   * cmd.add_metric("SetPoint", 75.0);
   * publisher.publish_device_command("Gateway01", "Motor01", cmd);
   * @endcode
   */
  [[nodiscard]] std::expected<void, std::string>
  publish_device_command(std::string_view target_edge_node_id, std::string_view target_device_id,
                         PayloadBuilder& payload);

private:
  /**
   * @brief Tracks state for an individual device attached to this edge node.
   */
  struct DeviceState {
    uint64_t seq_num{0};                     // Device message sequence (0-255)
    std::vector<uint8_t> last_birth_payload; // Last DBIRTH for rebirth
    bool is_online{false};                   // True if DBIRTH sent and device online
  };

  Config config_;
  MQTTAsyncHandle client_;
  uint64_t seq_num_{0};    // Node message sequence (0-255)
  uint64_t bd_seq_num_{0}; // Birth/Death sequence

  // Store the NDEATH payload for the MQTT Will
  std::vector<uint8_t> death_payload_data_;

  // Store last NBIRTH for rebirth command
  std::vector<uint8_t> last_birth_payload_;

  // Hash and equality functors that support heterogeneous lookup (string_view)
  struct StringHash {
    using is_transparent = void;
    [[nodiscard]] size_t operator()(std::string_view sv) const noexcept {
      return std::hash<std::string_view>{}(sv);
    }
  };

  struct StringEqual {
    using is_transparent = void;
    [[nodiscard]] bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
      return lhs == rhs;
    }
  };

  // Track state of attached devices (device_id -> state, with heterogeneous lookup)
  std::unordered_map<std::string, DeviceState, StringHash, StringEqual> device_states_;

  bool is_connected_{false};

  // Mutex for thread-safe access to all mutable state
  mutable std::mutex mutex_;

  [[nodiscard]] std::expected<void, std::string>
  publish_message(const Topic& topic, std::span<const uint8_t> payload_data);
};

} // namespace sparkplug