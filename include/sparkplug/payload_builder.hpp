// include/sparkplug/payload_builder.hpp
#pragma once

#include "datatype.hpp"
#include "sparkplug_b.pb.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sparkplug {

class PayloadBuilder {
public:
  PayloadBuilder();

  auto &&add_metric(this auto &&self, std::string_view name, auto &&value) {
    self.add_metric_impl(name, std::forward<decltype(value)>(value));
    return std::forward<decltype(self)>(self);
  }

  auto &&set_timestamp(this auto &&self, uint64_t ts) {
    self.payload_.set_timestamp(ts);
    return std::forward<decltype(self)>(self);
  }

  auto &&set_seq(this auto &&self, uint64_t seq) {
    self.payload_.set_seq(seq);
    return std::forward<decltype(self)>(self);
  }

  [[nodiscard]] std::vector<uint8_t> build() const;
  [[nodiscard]] const org::eclipse::tahu::protobuf::Payload &payload() const;

private:
  void add_metric_impl(std::string_view name, int32_t value);
  void add_metric_impl(std::string_view name, int64_t value);
  void add_metric_impl(std::string_view name, uint32_t value);
  void add_metric_impl(std::string_view name, uint64_t value);
  void add_metric_impl(std::string_view name, float value);
  void add_metric_impl(std::string_view name, double value);
  void add_metric_impl(std::string_view name, bool value);
  void add_metric_impl(std::string_view name, std::string_view value);

  org::eclipse::tahu::protobuf::Payload payload_;
};

} // namespace sparkplug