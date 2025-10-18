// src/c_bindings.cpp
#include "sparkplug/payload_builder.hpp"
#include "sparkplug/publisher.hpp"
#include "sparkplug/sparkplug_c.h"
#include <cstring>

struct sparkplug_publisher {
  sparkplug::Publisher impl;
};

struct sparkplug_payload {
  sparkplug::PayloadBuilder impl;
};

extern "C" {

sparkplug_publisher_t *sparkplug_publisher_create(const char *broker_url,
                                                  const char *client_id,
                                                  const char *group_id,
                                                  const char *edge_node_id) {
  sparkplug::Publisher::Config config{.broker_url = broker_url,
                                      .client_id = client_id,
                                      .group_id = group_id,
                                      .edge_node_id = edge_node_id};
  return new sparkplug_publisher{sparkplug::Publisher(std::move(config))};
}

void sparkplug_publisher_destroy(sparkplug_publisher_t *pub) { delete pub; }

int sparkplug_publisher_connect(sparkplug_publisher_t *pub) {
  return pub->impl.connect().has_value() ? 0 : -1;
}

int sparkplug_publisher_disconnect(sparkplug_publisher_t *pub) {
  return pub->impl.disconnect().has_value() ? 0 : -1;
}

sparkplug_payload_t *sparkplug_payload_create() {
  return new sparkplug_payload{sparkplug::PayloadBuilder()};
}

void sparkplug_payload_destroy(sparkplug_payload_t *payload) { delete payload; }

void sparkplug_payload_set_timestamp(sparkplug_payload_t *payload,
                                     uint64_t ts) {
  payload->impl.set_timestamp(ts);
}

void sparkplug_payload_set_seq(sparkplug_payload_t *payload, uint64_t seq) {
  payload->impl.set_seq(seq);
}

void sparkplug_payload_add_double(sparkplug_payload_t *payload,
                                  const char *name, double value) {
  payload->impl.add_metric(name, value);
}

size_t sparkplug_payload_serialize(const sparkplug_payload_t *payload,
                                   uint8_t *buffer, size_t buffer_size) {
  auto data = payload->impl.build();
  if (data.size() > buffer_size)
    return 0;
  std::memcpy(buffer, data.data(), data.size());
  return data.size();
}
}