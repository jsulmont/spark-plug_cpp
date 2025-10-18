// include/sparkplug/publisher.hpp
#pragma once

#include "payload_builder.hpp"
#include "topic.hpp"
#include <expected>
#include <memory>
#include <string>
#include <vector>

typedef void *MQTTAsync;

namespace sparkplug {

class Publisher {
public:
  struct Config {
    std::string broker_url;
    std::string client_id;
    std::string group_id;
    std::string edge_node_id;
    int qos = 1;
    bool clean_session = true;
    int keep_alive_interval = 60; // Sparkplug recommends 60 seconds
  };

  Publisher(Config config);
  ~Publisher();

  Publisher(const Publisher &) = delete;
  Publisher &operator=(const Publisher &) = delete;
  Publisher(Publisher &&) noexcept;
  Publisher &operator=(Publisher &&) noexcept;

  [[nodiscard]] std::expected<void, std::string> connect();
  [[nodiscard]] std::expected<void, std::string> disconnect();

  // NBIRTH - must be called first after connect
  [[nodiscard]] std::expected<void, std::string>
  publish_birth(PayloadBuilder &payload);

  // NDATA - publishes data, auto-manages sequence numbers
  [[nodiscard]] std::expected<void, std::string>
  publish_data(PayloadBuilder &payload);

  // NDEATH - graceful disconnect (usually automatic via LWT)
  [[nodiscard]] std::expected<void, std::string> publish_death();

  // Rebirth - triggers new NBIRTH with incremented bdSeq
  [[nodiscard]] std::expected<void, std::string> rebirth();

  // Get current sequence and bdSeq for monitoring
  [[nodiscard]] uint64_t get_seq() const { return seq_num_; }
  [[nodiscard]] uint64_t get_bd_seq() const { return bd_seq_num_; }

private:
  Config config_;
  MQTTAsync client_;
  uint64_t seq_num_{0};    // Message sequence (0-255)
  uint64_t bd_seq_num_{0}; // Birth/Death sequence

  // Store the NDEATH payload for the MQTT Will
  std::vector<uint8_t> death_payload_data_;

  // Store last NBIRTH for rebirth command
  std::vector<uint8_t> last_birth_payload_;

  bool is_connected_{false};

  [[nodiscard]] std::expected<void, std::string>
  publish_message(const Topic &topic, const std::vector<uint8_t> &payload_data);
};

} // namespace sparkplug