// include/sparkplug/topic.hpp
#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace sparkplug {

enum class MessageType {
  NBIRTH,
  NDEATH,
  DBIRTH,
  DDEATH,
  NDATA,
  DDATA,
  NCMD,
  DCMD,
  STATE
};

struct Topic {
  std::string group_id;
  MessageType message_type;
  std::string edge_node_id;
  std::string device_id;

  [[nodiscard]] std::string to_string() const;

  [[nodiscard]] static std::expected<Topic, std::string>
  parse(std::string_view topic_str);
};

} // namespace sparkplug