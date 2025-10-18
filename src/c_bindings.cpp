// src/c_bindings.cpp
#include "sparkplug/payload_builder.hpp"
#include "sparkplug/publisher.hpp"
#include "sparkplug/sparkplug_c.h"
#include "sparkplug/subscriber.hpp"
#include <cstring>
#include <memory>

struct sparkplug_publisher {
  sparkplug::Publisher impl;
};

struct sparkplug_subscriber {
  std::unique_ptr<sparkplug::Subscriber> impl;
  sparkplug_message_callback_t callback;
  void *user_data;
};

struct sparkplug_payload {
  sparkplug::PayloadBuilder impl;
};

extern "C" {

// ============================================================================
// Publisher Functions
// ============================================================================

sparkplug_publisher_t *sparkplug_publisher_create(const char *broker_url,
                                                  const char *client_id,
                                                  const char *group_id,
                                                  const char *edge_node_id) {
  if (!broker_url || !client_id || !group_id || !edge_node_id) {
    return nullptr;
  }

  sparkplug::Publisher::Config config{.broker_url = broker_url,
                                      .client_id = client_id,
                                      .group_id = group_id,
                                      .edge_node_id = edge_node_id};
  return new sparkplug_publisher{sparkplug::Publisher(std::move(config))};
}

void sparkplug_publisher_destroy(sparkplug_publisher_t *pub) { delete pub; }

int sparkplug_publisher_connect(sparkplug_publisher_t *pub) {
  if (!pub)
    return -1;
  return pub->impl.connect().has_value() ? 0 : -1;
}

int sparkplug_publisher_disconnect(sparkplug_publisher_t *pub) {
  if (!pub)
    return -1;
  return pub->impl.disconnect().has_value() ? 0 : -1;
}

int sparkplug_publisher_publish_birth(sparkplug_publisher_t *pub,
                                      const uint8_t *payload_data,
                                      size_t payload_len) {
  if (!pub || !payload_data)
    return -1;

  // Deserialize the payload
  org::eclipse::tahu::protobuf::Payload proto_payload;
  if (!proto_payload.ParseFromArray(payload_data,
                                    static_cast<int>(payload_len))) {
    return -1;
  }

  // Create PayloadBuilder from the deserialized payload
  sparkplug::PayloadBuilder builder;
  // Note: This is a simplified version - full implementation would need
  // to copy all metrics from proto_payload

  return pub->impl.publish_birth(builder).has_value() ? 0 : -1;
}

int sparkplug_publisher_publish_data(sparkplug_publisher_t *pub,
                                     const uint8_t *payload_data,
                                     size_t payload_len) {
  if (!pub || !payload_data)
    return -1;

  // Deserialize the payload
  org::eclipse::tahu::protobuf::Payload proto_payload;
  if (!proto_payload.ParseFromArray(payload_data,
                                    static_cast<int>(payload_len))) {
    return -1;
  }

  // Create PayloadBuilder from the deserialized payload
  sparkplug::PayloadBuilder builder;
  // Note: This is a simplified version - full implementation would need
  // to copy all metrics from proto_payload

  return pub->impl.publish_data(builder).has_value() ? 0 : -1;
}

int sparkplug_publisher_publish_death(sparkplug_publisher_t *pub) {
  if (!pub)
    return -1;
  return pub->impl.publish_death().has_value() ? 0 : -1;
}

sparkplug_subscriber_t *sparkplug_subscriber_create(
    const char *broker_url, const char *client_id, const char *group_id,
    sparkplug_message_callback_t callback, void *user_data) {

  if (!broker_url || !client_id || !group_id || !callback) {
    return nullptr;
  }

  auto *sub = new sparkplug_subscriber;
  sub->callback = callback;
  sub->user_data = user_data;

  // Create the C++ subscriber with a lambda that calls the C callback
  sparkplug::Subscriber::Config config{
      .broker_url = broker_url, .client_id = client_id, .group_id = group_id};

  auto message_handler =
      [sub](const sparkplug::Topic &topic,
            const org::eclipse::tahu::protobuf::Payload &payload) {
        // Serialize payload for C callback
        std::vector<uint8_t> data(payload.ByteSizeLong());
        payload.SerializeToArray(data.data(), static_cast<int>(data.size()));

        // Call C callback
        auto topic_str = topic.to_string();
        sub->callback(topic_str.c_str(), data.data(), data.size(),
                      sub->user_data);
      };

  sub->impl = std::make_unique<sparkplug::Subscriber>(
      std::move(config), std::move(message_handler));

  return sub;
}

void sparkplug_subscriber_destroy(sparkplug_subscriber_t *sub) { delete sub; }

int sparkplug_subscriber_connect(sparkplug_subscriber_t *sub) {
  if (!sub || !sub->impl)
    return -1;
  return sub->impl->connect().has_value() ? 0 : -1;
}

int sparkplug_subscriber_disconnect(sparkplug_subscriber_t *sub) {
  if (!sub || !sub->impl)
    return -1;
  return sub->impl->disconnect().has_value() ? 0 : -1;
}

int sparkplug_subscriber_subscribe_all(sparkplug_subscriber_t *sub) {
  if (!sub || !sub->impl)
    return -1;
  return sub->impl->subscribe_all().has_value() ? 0 : -1;
}

sparkplug_payload_t *sparkplug_payload_create() {
  return new sparkplug_payload{sparkplug::PayloadBuilder()};
}

void sparkplug_payload_destroy(sparkplug_payload_t *payload) { delete payload; }

void sparkplug_payload_set_timestamp(sparkplug_payload_t *payload,
                                     uint64_t ts) {
  if (payload) {
    payload->impl.set_timestamp(ts);
  }
}

void sparkplug_payload_set_seq(sparkplug_payload_t *payload, uint64_t seq) {
  if (payload) {
    payload->impl.set_seq(seq);
  }
}

void sparkplug_payload_add_int32(sparkplug_payload_t *payload, const char *name,
                                 int32_t value) {
  if (payload && name) {
    payload->impl.add_metric(name, value);
  }
}

void sparkplug_payload_add_int64(sparkplug_payload_t *payload, const char *name,
                                 int64_t value) {
  if (payload && name) {
    payload->impl.add_metric(name, value);
  }
}

void sparkplug_payload_add_float(sparkplug_payload_t *payload, const char *name,
                                 float value) {
  if (payload && name) {
    payload->impl.add_metric(name, value);
  }
}

void sparkplug_payload_add_double(sparkplug_payload_t *payload,
                                  const char *name, double value) {
  if (payload && name) {
    payload->impl.add_metric(name, value);
  }
}

void sparkplug_payload_add_bool(sparkplug_payload_t *payload, const char *name,
                                bool value) {
  if (payload && name) {
    payload->impl.add_metric(name, value);
  }
}

void sparkplug_payload_add_string(sparkplug_payload_t *payload,
                                  const char *name, const char *value) {
  if (payload && name && value) {
    payload->impl.add_metric(name, std::string_view(value));
  }
}

size_t sparkplug_payload_serialize(const sparkplug_payload_t *payload,
                                   uint8_t *buffer, size_t buffer_size) {
  if (!payload || !buffer)
    return 0;

  auto data = payload->impl.build();
  if (data.size() > buffer_size) {
    return 0;
  }

  std::memcpy(buffer, data.data(), data.size());
  return data.size();
}

} // extern "C"