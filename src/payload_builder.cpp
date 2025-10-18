// src/payload_builder.cpp
#include "sparkplug/payload_builder.hpp"
#include <chrono>

namespace sparkplug {

PayloadBuilder::PayloadBuilder() {
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch())
                .count();
  payload_.set_timestamp(ms);
}

void PayloadBuilder::add_metric_impl(std::string_view name, int32_t value) {
  auto *metric = payload_.add_metrics();
  metric->set_name(std::string(name));
  metric->set_datatype(static_cast<uint32_t>(DataType::Int32));
  metric->set_int_value(value);
}

void PayloadBuilder::add_metric_impl(std::string_view name, int64_t value) {
  auto *metric = payload_.add_metrics();
  metric->set_name(std::string(name));
  metric->set_datatype(static_cast<uint32_t>(DataType::Int64));
  metric->set_long_value(value);
}

void PayloadBuilder::add_metric_impl(std::string_view name, uint32_t value) {
  auto *metric = payload_.add_metrics();
  metric->set_name(std::string(name));
  metric->set_datatype(static_cast<uint32_t>(DataType::UInt32));
  metric->set_int_value(value);
}

void PayloadBuilder::add_metric_impl(std::string_view name, uint64_t value) {
  auto *metric = payload_.add_metrics();
  metric->set_name(std::string(name));
  metric->set_datatype(static_cast<uint32_t>(DataType::UInt64));
  metric->set_long_value(value);
}

void PayloadBuilder::add_metric_impl(std::string_view name, float value) {
  auto *metric = payload_.add_metrics();
  metric->set_name(std::string(name));
  metric->set_datatype(static_cast<uint32_t>(DataType::Float));
  metric->set_float_value(value);
}

void PayloadBuilder::add_metric_impl(std::string_view name, double value) {
  auto *metric = payload_.add_metrics();
  metric->set_name(std::string(name));
  metric->set_datatype(static_cast<uint32_t>(DataType::Double));
  metric->set_double_value(value);
}

void PayloadBuilder::add_metric_impl(std::string_view name, bool value) {
  auto *metric = payload_.add_metrics();
  metric->set_name(std::string(name));
  metric->set_datatype(static_cast<uint32_t>(DataType::Boolean));
  metric->set_boolean_value(value);
}

void PayloadBuilder::add_metric_impl(std::string_view name,
                                     std::string_view value) {
  auto *metric = payload_.add_metrics();
  metric->set_name(std::string(name));
  metric->set_datatype(static_cast<uint32_t>(DataType::String));
  metric->set_string_value(std::string(value));
}

std::vector<uint8_t> PayloadBuilder::build() const {
  std::vector<uint8_t> buffer(payload_.ByteSizeLong());
  payload_.SerializeToArray(buffer.data(), buffer.size());
  return buffer;
}

const org::eclipse::tahu::protobuf::Payload &PayloadBuilder::payload() const {
  return payload_;
}

} // namespace sparkplug