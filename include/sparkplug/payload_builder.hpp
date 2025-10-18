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
  if constexpr (std::is_same_v<BaseT, int32_t>)
    return DataType::Int32;
  else if constexpr (std::is_same_v<BaseT, int64_t>)
    return DataType::Int64;
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

  if constexpr (std::is_same_v<BaseT, int32_t> ||
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
    // Handle all string-like types: const char*, char arrays, std::string,
    // std::string_view
    metric->set_string_value(std::string(value));
  }
}

template <typename T>
void add_metric_to_payload(org::eclipse::tahu::protobuf::Payload &payload,
                           std::string_view name, T &&value,
                           std::optional<uint64_t> alias) {
  auto *metric = payload.add_metrics();

  if (!name.empty()) {
    metric->set_name(std::string(name));
  }
  if (alias.has_value()) {
    metric->set_alias(*alias);
  }

  metric->set_datatype(static_cast<uint32_t>(get_datatype<T>()));
  set_metric_value(metric, std::forward<T>(value));

  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now.time_since_epoch())
                       .count();
  metric->set_timestamp(timestamp);
}

} // namespace detail

class PayloadBuilder {
public:
  PayloadBuilder();

  // Add metric by name (for BIRTH messages)
  template <typename T>
  PayloadBuilder &add_metric(std::string_view name, T &&value) {
    detail::add_metric_to_payload(payload_, name, std::forward<T>(value),
                                  std::nullopt);
    return *this;
  }

  // Add metric by name with alias (for BIRTH messages)
  template <typename T>
  PayloadBuilder &add_metric_with_alias(std::string_view name, uint64_t alias,
                                        T &&value) {
    detail::add_metric_to_payload(payload_, name, std::forward<T>(value),
                                  alias);
    return *this;
  }

  // Add metric by alias only (for DATA messages after BIRTH)
  template <typename T>
  PayloadBuilder &add_metric_by_alias(uint64_t alias, T &&value) {
    detail::add_metric_to_payload(payload_, "", std::forward<T>(value), alias);
    return *this;
  }

  PayloadBuilder &set_timestamp(uint64_t ts) {
    payload_.set_timestamp(ts);
    return *this;
  }

  PayloadBuilder &set_seq(uint64_t seq) {
    payload_.set_seq(seq);
    seq_explicitly_set_ = true;
    return *this;
  }

  [[nodiscard]] bool has_seq() const { return seq_explicitly_set_; }

  [[nodiscard]] std::vector<uint8_t> build() const;
  [[nodiscard]] const org::eclipse::tahu::protobuf::Payload &payload() const;

  [[nodiscard]] org::eclipse::tahu::protobuf::Payload &mutable_payload() {
    return payload_;
  }

private:
  org::eclipse::tahu::protobuf::Payload payload_;
  bool seq_explicitly_set_{false};
};

} // namespace sparkplug