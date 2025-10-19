/**
 * @file sparkplug_c.h
 * @brief C API for Sparkplug B protocol implementation.
 *
 * This header provides a pure C interface to the Sparkplug B C++ library.
 * All functions return 0 on success, -1 on failure (unless otherwise specified).
 *
 * @par Example Usage
 * @code
 * // Create and use a publisher
 * sparkplug_publisher_t *pub = sparkplug_publisher_create(
 *     "tcp://localhost:1883", "client", "Energy", "Gateway01");
 * sparkplug_publisher_connect(pub);
 *
 * // Create payload and publish
 * sparkplug_payload_t *payload = sparkplug_payload_create();
 * sparkplug_payload_add_double_with_alias(payload, "Temperature", 1, 20.5);
 *
 * uint8_t buffer[4096];
 * size_t size = sparkplug_payload_serialize(payload, buffer, sizeof(buffer));
 * sparkplug_publisher_publish_birth(pub, buffer, size);
 *
 * sparkplug_payload_destroy(payload);
 * sparkplug_publisher_destroy(pub);
 * @endcode
 */
// include/sparkplug/sparkplug_c.h
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle to a Sparkplug publisher. */
typedef struct sparkplug_publisher sparkplug_publisher_t;

/** @brief Opaque handle to a Sparkplug subscriber. */
typedef struct sparkplug_subscriber sparkplug_subscriber_t;

/** @brief Opaque handle to a Sparkplug payload builder. */
typedef struct sparkplug_payload sparkplug_payload_t;

/**
 * @brief Callback function type for receiving Sparkplug messages.
 *
 * @param topic MQTT topic string
 * @param payload_data Raw protobuf payload data
 * @param payload_len Length of payload data in bytes
 * @param user_data User-provided context pointer
 */
typedef void (*sparkplug_message_callback_t)(const char* topic, const uint8_t* payload_data,
                                             size_t payload_len, void* user_data);

/* ============================================================================
 * Publisher API
 * ========================================================================= */

/**
 * @brief Creates a new Sparkplug publisher.
 *
 * @param broker_url MQTT broker URL (e.g., "tcp://localhost:1883")
 * @param client_id Unique MQTT client identifier
 * @param group_id Sparkplug group ID
 * @param edge_node_id Edge node identifier
 *
 * @return Publisher handle on success, NULL on failure
 *
 * @note Caller must call sparkplug_publisher_destroy() to free resources.
 */
sparkplug_publisher_t* sparkplug_publisher_create(const char* broker_url, const char* client_id,
                                                  const char* group_id, const char* edge_node_id);

/**
 * @brief Destroys a publisher and frees all resources.
 *
 * @param pub Publisher handle (may be NULL)
 */
void sparkplug_publisher_destroy(sparkplug_publisher_t* pub);

/**
 * @brief Connects the publisher to the MQTT broker.
 *
 * @param pub Publisher handle
 * @return 0 on success, -1 on failure
 */
int sparkplug_publisher_connect(sparkplug_publisher_t* pub);

/**
 * @brief Disconnects the publisher from the MQTT broker.
 *
 * @param pub Publisher handle
 * @return 0 on success, -1 on failure
 *
 * @note Sends NDEATH via MQTT Last Will Testament.
 */
int sparkplug_publisher_disconnect(sparkplug_publisher_t* pub);

/**
 * @brief Publishes an NBIRTH (Node Birth) message.
 *
 * @param pub Publisher handle
 * @param payload_data Serialized Sparkplug protobuf payload
 * @param payload_len Length of payload data in bytes
 *
 * @return 0 on success, -1 on failure
 *
 * @note Must be called after connect() and before any publish_data() calls.
 */
int sparkplug_publisher_publish_birth(sparkplug_publisher_t* pub, const uint8_t* payload_data,
                                      size_t payload_len);

/**
 * @brief Publishes an NDATA (Node Data) message.
 *
 * @param pub Publisher handle
 * @param payload_data Serialized Sparkplug protobuf payload
 * @param payload_len Length of payload data in bytes
 *
 * @return 0 on success, -1 on failure
 *
 * @note Sequence number is automatically incremented.
 */
int sparkplug_publisher_publish_data(sparkplug_publisher_t* pub, const uint8_t* payload_data,
                                     size_t payload_len);

/**
 * @brief Publishes an NDEATH (Node Death) message.
 *
 * @param pub Publisher handle
 * @return 0 on success, -1 on failure
 *
 * @note Usually not needed; NDEATH is sent automatically on disconnect.
 */
int sparkplug_publisher_publish_death(sparkplug_publisher_t* pub);

/**
 * @brief Triggers a rebirth (publishes new NBIRTH with incremented bdSeq).
 *
 * @param pub Publisher handle
 * @return 0 on success, -1 on failure
 */
int sparkplug_publisher_rebirth(sparkplug_publisher_t* pub);

/**
 * @brief Gets the current message sequence number.
 *
 * @param pub Publisher handle
 * @return Current sequence number (0-255)
 */
uint64_t sparkplug_publisher_get_seq(const sparkplug_publisher_t* pub);

/**
 * @brief Gets the current birth/death sequence number.
 *
 * @param pub Publisher handle
 * @return Current bdSeq value
 */
uint64_t sparkplug_publisher_get_bd_seq(const sparkplug_publisher_t* pub);

/* ============================================================================
 * Subscriber API
 * ========================================================================= */

/**
 * @brief Creates a new Sparkplug subscriber.
 *
 * @param broker_url MQTT broker URL (e.g., "tcp://localhost:1883")
 * @param client_id Unique MQTT client identifier
 * @param group_id Sparkplug group ID to subscribe to
 * @param callback Function to call for each received message
 * @param user_data User context pointer passed to callback
 *
 * @return Subscriber handle on success, NULL on failure
 *
 * @note Caller must call sparkplug_subscriber_destroy() to free resources.
 */
sparkplug_subscriber_t* sparkplug_subscriber_create(const char* broker_url, const char* client_id,
                                                    const char* group_id,
                                                    sparkplug_message_callback_t callback,
                                                    void* user_data);

/**
 * @brief Destroys a subscriber and frees all resources.
 *
 * @param sub Subscriber handle (may be NULL)
 */
void sparkplug_subscriber_destroy(sparkplug_subscriber_t* sub);

/**
 * @brief Connects the subscriber to the MQTT broker.
 *
 * @param sub Subscriber handle
 * @return 0 on success, -1 on failure
 */
int sparkplug_subscriber_connect(sparkplug_subscriber_t* sub);

/**
 * @brief Disconnects the subscriber from the MQTT broker.
 *
 * @param sub Subscriber handle
 * @return 0 on success, -1 on failure
 */
int sparkplug_subscriber_disconnect(sparkplug_subscriber_t* sub);

/**
 * @brief Subscribes to all Sparkplug messages in the configured group.
 *
 * @param sub Subscriber handle
 * @return 0 on success, -1 on failure
 */
int sparkplug_subscriber_subscribe_all(sparkplug_subscriber_t* sub);

/**
 * @brief Subscribes to messages from a specific edge node.
 *
 * @param sub Subscriber handle
 * @param edge_node_id Edge node ID to subscribe to
 *
 * @return 0 on success, -1 on failure
 */
int sparkplug_subscriber_subscribe_node(sparkplug_subscriber_t* sub, const char* edge_node_id);

/**
 * @brief Subscribes to STATE messages from a primary application.
 *
 * @param sub Subscriber handle
 * @param host_id Primary application host identifier
 *
 * @return 0 on success, -1 on failure
 */
int sparkplug_subscriber_subscribe_state(sparkplug_subscriber_t* sub, const char* host_id);

/* ============================================================================
 * Payload Builder API
 * ========================================================================= */

/**
 * @brief Creates a new payload builder.
 *
 * @return Payload handle on success, NULL on failure
 *
 * @note Caller must call sparkplug_payload_destroy() to free resources.
 */
sparkplug_payload_t* sparkplug_payload_create(void);

/**
 * @brief Destroys a payload builder and frees all resources.
 *
 * @param payload Payload handle (may be NULL)
 */
void sparkplug_payload_destroy(sparkplug_payload_t* payload);

/**
 * @brief Sets the payload-level timestamp.
 *
 * @param payload Payload handle
 * @param ts Timestamp in milliseconds since Unix epoch
 */
void sparkplug_payload_set_timestamp(sparkplug_payload_t* payload, uint64_t ts);

/**
 * @brief Sets the sequence number manually.
 *
 * @param payload Payload handle
 * @param seq Sequence number (0-255)
 *
 * @note Do not use in normal operation; Publisher manages this automatically.
 */
void sparkplug_payload_set_seq(sparkplug_payload_t* payload, uint64_t seq);

/* Metric functions by name */

/** @brief Adds an int8_t metric by name. */
void sparkplug_payload_add_int8(sparkplug_payload_t* payload, const char* name, int8_t value);
/** @brief Adds an int16_t metric by name. */
void sparkplug_payload_add_int16(sparkplug_payload_t* payload, const char* name, int16_t value);
/** @brief Adds an int32_t metric by name. */
void sparkplug_payload_add_int32(sparkplug_payload_t* payload, const char* name, int32_t value);
/** @brief Adds an int64_t metric by name. */
void sparkplug_payload_add_int64(sparkplug_payload_t* payload, const char* name, int64_t value);
/** @brief Adds a uint8_t metric by name. */
void sparkplug_payload_add_uint8(sparkplug_payload_t* payload, const char* name, uint8_t value);
/** @brief Adds a uint16_t metric by name. */
void sparkplug_payload_add_uint16(sparkplug_payload_t* payload, const char* name, uint16_t value);
/** @brief Adds a uint32_t metric by name. */
void sparkplug_payload_add_uint32(sparkplug_payload_t* payload, const char* name, uint32_t value);
/** @brief Adds a uint64_t metric by name. */
void sparkplug_payload_add_uint64(sparkplug_payload_t* payload, const char* name, uint64_t value);
/** @brief Adds a float metric by name. */
void sparkplug_payload_add_float(sparkplug_payload_t* payload, const char* name, float value);
/** @brief Adds a double metric by name. */
void sparkplug_payload_add_double(sparkplug_payload_t* payload, const char* name, double value);
/** @brief Adds a boolean metric by name. */
void sparkplug_payload_add_bool(sparkplug_payload_t* payload, const char* name, bool value);
/** @brief Adds a string metric by name. */
void sparkplug_payload_add_string(sparkplug_payload_t* payload, const char* name,
                                  const char* value);

/* Metric functions with alias */

/** @brief Adds an int32_t metric with both name and alias (for NBIRTH). */
void sparkplug_payload_add_int32_with_alias(sparkplug_payload_t* payload, const char* name,
                                            uint64_t alias, int32_t value);
/** @brief Adds an int64_t metric with both name and alias (for NBIRTH). */
void sparkplug_payload_add_int64_with_alias(sparkplug_payload_t* payload, const char* name,
                                            uint64_t alias, int64_t value);
/** @brief Adds a uint32_t metric with both name and alias (for NBIRTH). */
void sparkplug_payload_add_uint32_with_alias(sparkplug_payload_t* payload, const char* name,
                                             uint64_t alias, uint32_t value);
/** @brief Adds a uint64_t metric with both name and alias (for NBIRTH). */
void sparkplug_payload_add_uint64_with_alias(sparkplug_payload_t* payload, const char* name,
                                             uint64_t alias, uint64_t value);
/** @brief Adds a float metric with both name and alias (for NBIRTH). */
void sparkplug_payload_add_float_with_alias(sparkplug_payload_t* payload, const char* name,
                                            uint64_t alias, float value);
/** @brief Adds a double metric with both name and alias (for NBIRTH). */
void sparkplug_payload_add_double_with_alias(sparkplug_payload_t* payload, const char* name,
                                             uint64_t alias, double value);
/** @brief Adds a boolean metric with both name and alias (for NBIRTH). */
void sparkplug_payload_add_bool_with_alias(sparkplug_payload_t* payload, const char* name,
                                           uint64_t alias, bool value);

/* Metric functions by alias only */

/** @brief Adds an int32_t metric by alias only (for NDATA). */
void sparkplug_payload_add_int32_by_alias(sparkplug_payload_t* payload, uint64_t alias,
                                          int32_t value);
/** @brief Adds an int64_t metric by alias only (for NDATA). */
void sparkplug_payload_add_int64_by_alias(sparkplug_payload_t* payload, uint64_t alias,
                                          int64_t value);
/** @brief Adds a uint32_t metric by alias only (for NDATA). */
void sparkplug_payload_add_uint32_by_alias(sparkplug_payload_t* payload, uint64_t alias,
                                           uint32_t value);
/** @brief Adds a uint64_t metric by alias only (for NDATA). */
void sparkplug_payload_add_uint64_by_alias(sparkplug_payload_t* payload, uint64_t alias,
                                           uint64_t value);
/** @brief Adds a float metric by alias only (for NDATA). */
void sparkplug_payload_add_float_by_alias(sparkplug_payload_t* payload, uint64_t alias,
                                          float value);
/** @brief Adds a double metric by alias only (for NDATA). */
void sparkplug_payload_add_double_by_alias(sparkplug_payload_t* payload, uint64_t alias,
                                           double value);
/** @brief Adds a boolean metric by alias only (for NDATA). */
void sparkplug_payload_add_bool_by_alias(sparkplug_payload_t* payload, uint64_t alias, bool value);

/**
 * @brief Serializes the payload to a binary Protocol Buffers format.
 *
 * @param payload Payload handle
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer in bytes
 *
 * @return Number of bytes written on success, 0 on failure
 *
 * @note The serialized data can be passed to publish_birth() or publish_data().
 */
size_t sparkplug_payload_serialize(const sparkplug_payload_t* payload, uint8_t* buffer,
                                   size_t buffer_size);

#ifdef __cplusplus
}
#endif
