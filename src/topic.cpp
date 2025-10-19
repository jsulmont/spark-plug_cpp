// src/topic.cpp
#include "sparkplug/topic.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <utility>
#include <vector>

namespace sparkplug {

namespace {

using namespace std::string_view_literals;
constexpr auto NAMESPACE = "spBv1.0"sv;

constexpr std::string_view message_type_to_string(MessageType type) {
  switch (type) {
  case MessageType::NBIRTH:
    return "NBIRTH";
  case MessageType::NDEATH:
    return "NDEATH";
  case MessageType::DBIRTH:
    return "DBIRTH";
  case MessageType::DDEATH:
    return "DDEATH";
  case MessageType::NDATA:
    return "NDATA";
  case MessageType::DDATA:
    return "DDATA";
  case MessageType::NCMD:
    return "NCMD";
  case MessageType::DCMD:
    return "DCMD";
  case MessageType::STATE:
    return "STATE";
  }
  std::unreachable();
}

std::expected<MessageType, std::string> parse_message_type(std::string_view str) {
  if (str == "NBIRTH")
    return MessageType::NBIRTH;
  if (str == "NDEATH")
    return MessageType::NDEATH;
  if (str == "DBIRTH")
    return MessageType::DBIRTH;
  if (str == "DDEATH")
    return MessageType::DDEATH;
  if (str == "NDATA")
    return MessageType::NDATA;
  if (str == "DDATA")
    return MessageType::DDATA;
  if (str == "NCMD")
    return MessageType::NCMD;
  if (str == "DCMD")
    return MessageType::DCMD;
  if (str == "STATE")
    return MessageType::STATE;
  return std::unexpected(std::format("Unknown message type: {}", str));
}
} // namespace

std::string Topic::to_string() const {
  if (message_type == MessageType::STATE) {
    return std::format("STATE/{}", edge_node_id);
  }

  auto base = std::format("{}/{}/{}/{}", NAMESPACE, group_id, message_type_to_string(message_type),
                          edge_node_id);

  if (!device_id.empty()) {
    return std::format("{}/{}", base, device_id);
  }
  return base;
}

std::expected<Topic, std::string> Topic::parse(std::string_view topic_str) {
  auto parts = topic_str | std::views::split('/') |
               std::views::transform([](auto&& rng) { return std::string_view(rng); });
  auto elements = parts | std::ranges::to<std::vector>();
  if (elements.size() < 2) {
    return std::unexpected("Invalid topic format");
  }

  if (elements[0] == "STATE") {
    return Topic{.group_id = "",
                 .message_type = MessageType::STATE,
                 .edge_node_id = std::string(elements[1]),
                 .device_id = ""};
  }

  if (elements.size() < 4 || elements[0] != NAMESPACE) {
    return std::unexpected("Invalid Sparkplug B topic");
  }

  auto msg_type = parse_message_type(elements[2]);
  if (!msg_type) {
    return std::unexpected(msg_type.error());
  }

  return Topic{.group_id = std::string(elements[1]),
               .message_type = *msg_type,
               .edge_node_id = std::string(elements[3]),
               .device_id = elements.size() > 4 ? std::string(elements[4]) : ""};
}

} // namespace sparkplug