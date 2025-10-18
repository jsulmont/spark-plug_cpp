// include/sparkplug/subscriber.hpp
#pragma once

#include "sparkplug_b.pb.h"
#include "topic.hpp"
#include <expected>
#include <functional>
#include <memory>
#include <string>

struct MQTTAsync_struct;
typedef MQTTAsync_struct *MQTTAsync;

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
  };

  Subscriber(Config config, MessageCallback callback);
  ~Subscriber();

  Subscriber(const Subscriber &) = delete;
  Subscriber &operator=(const Subscriber &) = delete;
  Subscriber(Subscriber &&) noexcept;
  Subscriber &operator=(Subscriber &&) noexcept;

  [[nodiscard]] std::expected<void, std::string> connect();
  [[nodiscard]] std::expected<void, std::string> disconnect();

  [[nodiscard]] std::expected<void, std::string> subscribe_all();

  [[nodiscard]] std::expected<void, std::string>
  subscribe_node(std::string_view edge_node_id);

private:
  Config config_;
  MessageCallback callback_;
  MQTTAsync client_;
};

} // namespace sparkplug