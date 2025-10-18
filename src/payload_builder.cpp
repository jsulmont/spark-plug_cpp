// src/payload_builder.cpp
#include "sparkplug/payload_builder.hpp"

namespace sparkplug {

PayloadBuilder::PayloadBuilder() = default;

std::vector<uint8_t> PayloadBuilder::build() const {
  std::vector<uint8_t> buffer(payload_.ByteSizeLong());
  payload_.SerializeToArray(buffer.data(), static_cast<int>(buffer.size()));
  return buffer;
}

const org::eclipse::tahu::protobuf::Payload &PayloadBuilder::payload() const {
  return payload_;
}

} // namespace sparkplug