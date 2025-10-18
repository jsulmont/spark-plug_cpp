// include/sparkplug/publisher.hpp
#pragma once

#include "payload_builder.hpp"
#include "topic.hpp"
#include <expected>
#include <memory>
#include <string>

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
  };

  Publisher(Config config);
  ~Publisher();

  Publisher(const Publisher &) = delete;
  Publisher &operator=(const Publisher &) = delete;
  Publisher(Publisher &&) noexcept;
  Publisher &operator=(Publisher &&) noexcept;

  [[nodiscard]] std::expected<void, std::string> connect();
  [[nodiscard]] std::expected<void, std::string> disconnect();

  [[nodiscard]] std::expected<void, std::string>
  publish_birth(const PayloadBuilder &payload);

  [[nodiscard]] std::expected<void, std::string>
  publish_data(const PayloadBuilder &payload);

  [[nodiscard]] std::expected<void, std::string> publish_death();

private:
  Config config_;
  MQTTAsync client_;
  uint64_t seq_num_{0};
  uint64_t bd_seq_num_{0};
};

} // namespace sparkplug