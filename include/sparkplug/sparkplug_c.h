// include/sparkplug/sparkplug_c.h
#pragma once

#include <stdbool.h>
#include <stddef.h>
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

int sparkplug_publisher_rebirth(sparkplug_publisher_t *pub);

uint64_t sparkplug_publisher_get_seq(const sparkplug_publisher_t *pub);
uint64_t sparkplug_publisher_get_bd_seq(const sparkplug_publisher_t *pub);

sparkplug_subscriber_t *sparkplug_subscriber_create(
    const char *broker_url, const char *client_id, const char *group_id,
    sparkplug_message_callback_t callback, void *user_data);

void sparkplug_subscriber_destroy(sparkplug_subscriber_t *sub);

int sparkplug_subscriber_connect(sparkplug_subscriber_t *sub);
int sparkplug_subscriber_disconnect(sparkplug_subscriber_t *sub);
int sparkplug_subscriber_subscribe_all(sparkplug_subscriber_t *sub);
int sparkplug_subscriber_subscribe_node(sparkplug_subscriber_t *sub,
                                         const char *edge_node_id);
int sparkplug_subscriber_subscribe_state(sparkplug_subscriber_t *sub,
                                          const char *host_id);

sparkplug_payload_t *sparkplug_payload_create(void);
void sparkplug_payload_destroy(sparkplug_payload_t *payload);

void sparkplug_payload_set_timestamp(sparkplug_payload_t *payload, uint64_t ts);
void sparkplug_payload_set_seq(sparkplug_payload_t *payload, uint64_t seq);

// Add metrics by name (for NBIRTH)
void sparkplug_payload_add_int8(sparkplug_payload_t *payload, const char *name, int8_t value);
void sparkplug_payload_add_int16(sparkplug_payload_t *payload, const char *name, int16_t value);
void sparkplug_payload_add_int32(sparkplug_payload_t *payload, const char *name, int32_t value);
void sparkplug_payload_add_int64(sparkplug_payload_t *payload, const char *name, int64_t value);
void sparkplug_payload_add_uint8(sparkplug_payload_t *payload, const char *name, uint8_t value);
void sparkplug_payload_add_uint16(sparkplug_payload_t *payload, const char *name, uint16_t value);
void sparkplug_payload_add_uint32(sparkplug_payload_t *payload, const char *name, uint32_t value);
void sparkplug_payload_add_uint64(sparkplug_payload_t *payload, const char *name, uint64_t value);
void sparkplug_payload_add_float(sparkplug_payload_t *payload, const char *name, float value);
void sparkplug_payload_add_double(sparkplug_payload_t *payload, const char *name, double value);
void sparkplug_payload_add_bool(sparkplug_payload_t *payload, const char *name, bool value);
void sparkplug_payload_add_string(sparkplug_payload_t *payload, const char *name, const char *value);

// Add metrics with alias (for NBIRTH with aliases)
void sparkplug_payload_add_int32_with_alias(sparkplug_payload_t *payload, const char *name, uint64_t alias, int32_t value);
void sparkplug_payload_add_int64_with_alias(sparkplug_payload_t *payload, const char *name, uint64_t alias, int64_t value);
void sparkplug_payload_add_uint32_with_alias(sparkplug_payload_t *payload, const char *name, uint64_t alias, uint32_t value);
void sparkplug_payload_add_uint64_with_alias(sparkplug_payload_t *payload, const char *name, uint64_t alias, uint64_t value);
void sparkplug_payload_add_float_with_alias(sparkplug_payload_t *payload, const char *name, uint64_t alias, float value);
void sparkplug_payload_add_double_with_alias(sparkplug_payload_t *payload, const char *name, uint64_t alias, double value);
void sparkplug_payload_add_bool_with_alias(sparkplug_payload_t *payload, const char *name, uint64_t alias, bool value);

// Add metrics by alias only (for NDATA)
void sparkplug_payload_add_int32_by_alias(sparkplug_payload_t *payload, uint64_t alias, int32_t value);
void sparkplug_payload_add_int64_by_alias(sparkplug_payload_t *payload, uint64_t alias, int64_t value);
void sparkplug_payload_add_uint32_by_alias(sparkplug_payload_t *payload, uint64_t alias, uint32_t value);
void sparkplug_payload_add_uint64_by_alias(sparkplug_payload_t *payload, uint64_t alias, uint64_t value);
void sparkplug_payload_add_float_by_alias(sparkplug_payload_t *payload, uint64_t alias, float value);
void sparkplug_payload_add_double_by_alias(sparkplug_payload_t *payload, uint64_t alias, double value);
void sparkplug_payload_add_bool_by_alias(sparkplug_payload_t *payload, uint64_t alias, bool value);

size_t sparkplug_payload_serialize(const sparkplug_payload_t *payload,
                                   uint8_t *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif