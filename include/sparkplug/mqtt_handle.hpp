// include/sparkplug/mqtt_handle.hpp
#pragma once

#include <memory>

typedef void *MQTTAsync;

namespace sparkplug {

// Custom deleter for MQTTAsync client RAII
struct MQTTAsyncDeleter {
  void operator()(MQTTAsync *client) const noexcept;
};

using MQTTAsyncHandle = std::unique_ptr<MQTTAsync, MQTTAsyncDeleter>;

} // namespace sparkplug
