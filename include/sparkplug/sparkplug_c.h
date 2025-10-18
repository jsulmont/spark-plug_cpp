// include/sparkplug/sparkplug_c.h
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sparkplug_publisher sparkplug_publisher_t;
typedef struct sparkplug_subscriber sparkplug_subscriber_t;
typedef struct sparkplug_payload sparkplug_payload_t;

typedef void (*sparkplug_message_callback_t)(const char *topic,
                                             const uint8_t *payload_data,
                                             size_t payload_len,
                                             void *user_data);

sparkplug_publisher_t *sparkplug_publisher_create(const char *broker_url,
                                                  const char *client_id,
                                                  const char *group_id,
                                                  const char *edge_node_id);

void sparkplug_publisher_destroy(sparkplug_publisher_t *pub);

int sparkplug_publisher_connect(sparkplug_publisher_t *pub);
int sparkplug_publisher_disconnect(sparkplug_publisher_t *pub);

int sparkplug_publisher_publish_birth(sparkplug_publisher_t *pub,
                                      const uint8_t *payload_data,
                                      size_t payload_len);

int sparkplug_publisher_publish_data(sparkplug_publisher_t *pub,
                                     const uint8_t *payload_data,
                                     size_t payload_len);

int sparkplug_publisher_publish_death(sparkplug_publisher_t *pub);

sparkplug_subscriber_t *sparkplug_subscriber_create(
    const char *broker_url, const char *client_id, const char *group_id,
    sparkplug_message_callback_t callback, void *user_data);

void sparkplug_subscriber_destroy(sparkplug_subscriber_t *sub);

int sparkplug_subscriber_connect(sparkplug_subscriber_t *sub);
int sparkplug_subscriber_disconnect(sparkplug_subscriber_t *sub);
int sparkplug_subscriber_subscribe_all(sparkplug_subscriber_t *sub);

sparkplug_payload_t *sparkplug_payload_create(void);
void sparkplug_payload_destroy(sparkplug_payload_t *payload);

void sparkplug_payload_set_timestamp(sparkplug_payload_t *payload, uint64_t ts);
void sparkplug_payload_set_seq(sparkplug_payload_t *payload, uint64_t seq);

void sparkplug_payload_add_int32(sparkplug_payload_t *payload, const char *name,
                                 int32_t value);

void sparkplug_payload_add_int64(sparkplug_payload_t *payload, const char *name,
                                 int64_t value);

void sparkplug_payload_add_float(sparkplug_payload_t *payload, const char *name,
                                 float value);

void sparkplug_payload_add_double(sparkplug_payload_t *payload,
                                  const char *name, double value);

void sparkplug_payload_add_bool(sparkplug_payload_t *payload, const char *name,
                                bool value);

void sparkplug_payload_add_string(sparkplug_payload_t *payload,
                                  const char *name, const char *value);

size_t sparkplug_payload_serialize(const sparkplug_payload_t *payload,
                                   uint8_t *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif