// include/sparkplug/payload_builder.hpp
#pragma once

#include "datatype.hpp"
#include "sparkplug_b.pb.h"
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace sparkplug {

namespace detail {

template <typename T> constexpr DataType get_datatype() {
  using BaseT = std::remove_cvref_t<T>;
  if constexpr (std::is_same_v<BaseT, int8_t>)
    return DataType::Int8;
  else if constexpr (std::is_same_v<BaseT, int16_t>)
    return DataType::Int16;
  else if constexpr (std::is_same_v<BaseT, int32_t>)
    return DataType::Int32;
  else if constexpr (std::is_same_v<BaseT, int64_t>)
    return DataType::Int64;
  else if constexpr (std::is_same_v<BaseT, uint8_t>)
    return DataType::UInt8;
  else if constexpr (std::is_same_v<BaseT, uint16_t>)
    return DataType::UInt16;
  else if constexpr (std::is_same_v<BaseT, uint32_t>)
    return DataType::UInt32;
  else if constexpr (std::is_same_v<BaseT, uint64_t>)
    return DataType::UInt64;
  else if constexpr (std::is_same_v<BaseT, float>)
    return DataType::Float;
  else if constexpr (std::is_same_v<BaseT, double>)
    return DataType::Double;
  else if constexpr (std::is_same_v<BaseT, bool>)
    return DataType::Boolean;
  else
    return DataType::String;
}

template <typename T>
void set_metric_value(org::eclipse::tahu::protobuf::Payload::Metric *metric,
                      T &&value) {
  using BaseT = std::remove_cvref_t<T>;

  if constexpr (std::is_same_v<BaseT, int8_t> ||
                std::is_same_v<BaseT, int16_t> ||
                std::is_same_v<BaseT, int32_t> ||
                std::is_same_v<BaseT, uint8_t> ||
                std::is_same_v<BaseT, uint16_t> ||
                std::is_same_v<BaseT, uint32_t>) {
    metric->set_int_value(value);
  } else if constexpr (std::is_same_v<BaseT, int64_t> ||
                       std::is_same_v<BaseT, uint64_t>) {
    metric->set_long_value(value);
  } else if constexpr (std::is_same_v<BaseT, float>) {
    metric->set_float_value(value);
  } else if constexpr (std::is_same_v<BaseT, double>) {
    metric->set_double_value(value);
  } else if constexpr (std::is_same_v<BaseT, bool>) {
    metric->set_boolean_value(value);
  } else {
    // Handle all string-like types
    metric->set_string_value(std::string(value));
  }
}

template <typename T>
void add_metric_to_payload(org::eclipse::tahu::protobuf::Payload &payload,
                           std::string_view name, T &&value,
                           std::optional<uint64_t> alias,
                           std::optional<uint64_t> timestamp_ms) {
  auto *metric = payload.add_metrics();

  if (!name.empty()) {
    metric->set_name(std::string(name));
  }
  if (alias.has_value()) {
    metric->set_alias(*alias);
  }

  metric->set_datatype(static_cast<uint32_t>(get_datatype<T>()));
  set_metric_value(metric, std::forward<T>(value));

  // Use provided timestamp or current time
  uint64_t ts;
  if (timestamp_ms.has_value()) {
    ts = *timestamp_ms;
  } else {
    auto now = std::chrono::system_clock::now();
    ts = std::chrono::duration_cast<std::chrono::milliseconds>(
             now.time_since_epoch())
             .count();
  }
  metric->set_timestamp(ts);
}

} // namespace detail

class PayloadBuilder {
public:
  PayloadBuilder();

  // Add metric by name (for BIRTH messages)
  template <typename T>
  PayloadBuilder &add_metric(std::string_view name, T &&value) {
    detail::add_metric_to_payload(payload_, name, std::forward<T>(value),
                                  std::nullopt, std::nullopt);
    return *this;
  }

  // Add metric by name with custom timestamp
  template <typename T>
  PayloadBuilder &add_metric(std::string_view name, T &&value,
                             uint64_t timestamp_ms) {
    detail::add_metric_to_payload(payload_, name, std::forward<T>(value),
                                  std::nullopt, timestamp_ms);
    return *this;
  }

  // Add metric by name with alias (for BIRTH messages)
  template <typename T>
  PayloadBuilder &add_metric_with_alias(std::string_view name, uint64_t alias,
                                        T &&value) {
    detail::add_metric_to_payload(payload_, name, std::forward<T>(value), alias,
                                  std::nullopt);
    return *this;
  }

  // Add metric by alias only (for DATA messages after BIRTH)
  template <typename T>
  PayloadBuilder &add_metric_by_alias(uint64_t alias, T &&value) {
    detail::add_metric_to_payload(payload_, "", std::forward<T>(value), alias,
                                  std::nullopt);
    return *this;
  }

  // Add metric by alias with custom timestamp (for historical data)
  template <typename T>
  PayloadBuilder &add_metric_by_alias(uint64_t alias, T &&value,
                                      uint64_t timestamp_ms) {
    detail::add_metric_to_payload(payload_, "", std::forward<T>(value), alias,
                                  timestamp_ms);
    return *this;
  }

  // Set payload timestamp (when message was created)
  PayloadBuilder &set_timestamp(uint64_t ts) {
    payload_.set_timestamp(ts);
    timestamp_explicitly_set_ = true;
    return *this;
  }

  // Set sequence number (0-255)
  PayloadBuilder &set_seq(uint64_t seq) {
    payload_.set_seq(seq);
    seq_explicitly_set_ = true;
    return *this;
  }

  // Add Node Control metrics (convenience methods for NBIRTH)
  PayloadBuilder &add_node_control_rebirth(bool value = false) {
    add_metric("Node Control/Rebirth", value);
    return *this;
  }

  PayloadBuilder &add_node_control_reboot(bool value = false) {
    add_metric("Node Control/Reboot", value);
    return *this;
  }

  PayloadBuilder &add_node_control_next_server(bool value = false) {
    add_metric("Node Control/Next Server", value);
    return *this;
  }

  PayloadBuilder &add_node_control_scan_rate(int64_t value) {
    add_metric("Node Control/Scan Rate", value);
    return *this;
  }

  // Query methods
  [[nodiscard]] bool has_seq() const { return seq_explicitly_set_; }
  [[nodiscard]] bool has_timestamp() const { return timestamp_explicitly_set_; }

  // Build and access
  [[nodiscard]] std::vector<uint8_t> build() const;
  [[nodiscard]] const org::eclipse::tahu::protobuf::Payload &payload() const;
  [[nodiscard]] org::eclipse::tahu::protobuf::Payload &mutable_payload() {
    return payload_;
  }

private:
  org::eclipse::tahu::protobuf::Payload payload_;
  bool seq_explicitly_set_{false};
  bool timestamp_explicitly_set_{false};
};

} // namespace sparkplug