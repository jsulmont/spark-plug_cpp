// src/subscriber.cpp
#include "sparkplug/subscriber.hpp"

namespace sparkplug {

Subscriber::Subscriber(Config config, MessageCallback callback)
    : config_(std::move(config)), callback_(std::move(callback)),
      client_(nullptr) {}

Subscriber::~Subscriber() = default;

Subscriber::Subscriber(Subscriber &&) noexcept = default;
Subscriber &Subscriber::operator=(Subscriber &&) noexcept = default;

std::expected<void, std::string> Subscriber::connect() {
  return std::unexpected("Not implemented");
}

std::expected<void, std::string> Subscriber::disconnect() {
  return std::unexpected("Not implemented");
}

std::expected<void, std::string> Subscriber::subscribe_all() {
  return std::unexpected("Not implemented");
}

std::expected<void, std::string> Subscriber::subscribe_node(std::string_view) {
  return std::unexpected("Not implemented");
}

} // namespace sparkplug