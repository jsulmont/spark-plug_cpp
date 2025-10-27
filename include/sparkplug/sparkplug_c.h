/**
 * @file sparkplug_c.h
 * @brief C API for Sparkplug B protocol implementation.
 *
 * This header provides a pure C interface to the Sparkplug B C++ library.
 * All functions return 0 on success, -1 on failure (unless otherwise specified).
 *
 * @par Thread Safety
 * All functions in this API are thread-safe. Multiple threads may call any
 * function concurrently on the same or different handles. Internal synchronization
 * is handled automatically via mutex locking in the underlying C++ implementation.
 *
 * This enables safe usage in multi-threaded applications, including:
 * - Publishing from multiple threads simultaneously
 * - Sharing publisher/subscriber handles across threads
 * - Concurrent calls to get_seq(), get_bd_seq(), etc.
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
 * @brief Log severity levels for library diagnostics.
 */
typedef enum {
  SPARKPLUG_LOG_DEBUG = 0, /**< Detailed debugging information */
  SPARKPLUG_LOG_INFO = 1,  /**< Informational messages */
  SPARKPLUG_LOG_WARN = 2,  /**< Warning messages (potential issues) */
  SPARKPLUG_LOG_ERROR = 3  /**< Error messages (serious problems) */
} sparkplug_log_level_t;

/**
 * @brief Callback function type for receiving log messages from the library.
 *
 * The library will call this function to report warnings, errors, and debug information.
 * If no callback is set, logging is silently disabled (zero overhead).
 *
 * @param level Log severity level
 * @param message Log message (null-terminated string)
 * @param message_len Length of message in bytes (excluding null terminator)
 * @param user_data User-provided context pointer
 *
 * @par Example Usage (integrating with syslog)
 * @code
 * void my_log_callback(int level, const char* msg, size_t len, void* user_data) {
 *     if (level >= SPARKPLUG_LOG_WARN) {
 *         syslog(LOG_WARNING, "[sparkplug] %.*s", (int)len, msg);
 *     }
 * }
 * @endcode
 *
 * @par Example Usage (Rust FFI)
 * @code{.rust}
 * extern "C" fn log_callback(level: c_int, msg: *const c_char, len: usize, _user_data: *mut c_void)
 * { let level = match level { 0 => log::Level::Debug, 1 => log::Level::Info, 2 => log::Level::Warn,
 *         _ => log::Level::Error,
 *     };
 *     let msg = unsafe { std::slice::from_raw_parts(msg as *const u8, len) };
 *     log::log!(level, "{}", String::from_utf8_lossy(msg));
 * }
 * @endcode
 */
typedef void (*sparkplug_log_callback_t)(int level, const char* message, size_t message_len,
                                         void* user_data);

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

/**
 * @brief Callback function type for Sparkplug command messages (NCMD/DCMD).
 *
 * @param topic MQTT topic string
 * @param payload_data Raw protobuf payload data
 * @param payload_len Length of payload data in bytes
 * @param user_data User-provided context pointer
 */
typedef void (*sparkplug_command_callback_t)(const char* topic, const uint8_t* payload_data,
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
 * @brief Sets MQTT username and password for authentication.
 *
 * @param pub Publisher handle
 * @param username MQTT username (may be NULL to unset)
 * @param password MQTT password (may be NULL to unset)
 * @return 0 on success, -1 on failure
 *
 * @note Must be called before sparkplug_publisher_connect().
 */
int sparkplug_publisher_set_credentials(sparkplug_publisher_t* pub, const char* username,
                                        const char* password);

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

/**
 * @brief Publishes a DBIRTH (Device Birth) message for a device.
 *
 * @param pub Publisher handle
 * @param device_id Device identifier (e.g., "Sensor01", "Motor02")
 * @param payload_data Serialized Sparkplug protobuf payload
 * @param payload_len Length of payload data in bytes
 *
 * @return 0 on success, -1 on failure
 *
 * @note Must call publish_birth() before publishing any device births.
 */
int sparkplug_publisher_publish_device_birth(sparkplug_publisher_t* pub, const char* device_id,
                                             const uint8_t* payload_data, size_t payload_len);

/**
 * @brief Publishes a DDATA (Device Data) message for a device.
 *
 * @param pub Publisher handle
 * @param device_id Device identifier
 * @param payload_data Serialized Sparkplug protobuf payload
 * @param payload_len Length of payload data in bytes
 *
 * @return 0 on success, -1 on failure
 *
 * @note Must call publish_device_birth() before the first publish_device_data().
 */
int sparkplug_publisher_publish_device_data(sparkplug_publisher_t* pub, const char* device_id,
                                            const uint8_t* payload_data, size_t payload_len);

/**
 * @brief Publishes a DDEATH (Device Death) message for a device.
 *
 * @param pub Publisher handle
 * @param device_id Device identifier
 *
 * @return 0 on success, -1 on failure
 */
int sparkplug_publisher_publish_device_death(sparkplug_publisher_t* pub, const char* device_id);

/**
 * @brief Publishes an NCMD (Node Command) message to another edge node.
 *
 * @param pub Publisher handle
 * @param target_edge_node_id Target edge node identifier
 * @param payload_data Serialized Sparkplug protobuf payload
 * @param payload_len Length of payload data in bytes
 *
 * @return 0 on success, -1 on failure
 */
int sparkplug_publisher_publish_node_command(sparkplug_publisher_t* pub,
                                             const char* target_edge_node_id,
                                             const uint8_t* payload_data, size_t payload_len);

/**
 * @brief Publishes a DCMD (Device Command) message to a device on another edge node.
 *
 * @param pub Publisher handle
 * @param target_edge_node_id Target edge node identifier
 * @param target_device_id Target device identifier
 * @param payload_data Serialized Sparkplug protobuf payload
 * @param payload_len Length of payload data in bytes
 *
 * @return 0 on success, -1 on failure
 */
int sparkplug_publisher_publish_device_command(sparkplug_publisher_t* pub,
                                               const char* target_edge_node_id,
                                               const char* target_device_id,
                                               const uint8_t* payload_data, size_t payload_len);

/**
 * @brief Publishes a STATE birth message for a Host Application.
 *
 * STATE messages are used by Host Applications (SCADA/Primary Applications) to
 * indicate their online status. The birth message declares the Host Application is online.
 *
 * @param pub Publisher handle
 * @param host_id Host application identifier (e.g., "SCADA01", "HostApp")
 * @param timestamp UTC milliseconds since epoch
 *
 * @return 0 on success, -1 on failure
 *
 * @note Topic format: STATE/<host_id>
 * @note Payload format: JSON {"online": true, "timestamp": <timestamp>}
 * @note Message is published with Retain=true and QoS=1
 * @note This is NOT a Sparkplug protobuf message - uses raw JSON payload
 *
 * @par Example Usage
 * @code
 * uint64_t timestamp = time(NULL) * 1000; // Current time in milliseconds
 * sparkplug_publisher_publish_state_birth(pub, "SCADA01", timestamp);
 * @endcode
 *
 * @see sparkplug_publisher_publish_state_death() for declaring Host Application offline
 */
int sparkplug_publisher_publish_state_birth(sparkplug_publisher_t* pub, const char* host_id,
                                            uint64_t timestamp);

/**
 * @brief Publishes a STATE death message for a Host Application.
 *
 * STATE messages are used by Host Applications (SCADA/Primary Applications) to
 * indicate their online status. The death message declares the Host Application is offline.
 *
 * @param pub Publisher handle
 * @param host_id Host application identifier (e.g., "SCADA01", "HostApp")
 * @param timestamp UTC milliseconds since epoch (must match birth timestamp)
 *
 * @return 0 on success, -1 on failure
 *
 * @note Topic format: STATE/<host_id>
 * @note Payload format: JSON {"online": false, "timestamp": <timestamp>}
 * @note Message is published with Retain=true and QoS=1
 * @note This is NOT a Sparkplug protobuf message - uses raw JSON payload
 *
 * @par Example Usage
 * @code
 * uint64_t timestamp = time(NULL) * 1000; // Current time in milliseconds
 * sparkplug_publisher_publish_state_death(pub, "SCADA01", timestamp);
 * @endcode
 *
 * @see sparkplug_publisher_publish_state_birth() for declaring Host Application online
 */
int sparkplug_publisher_publish_state_death(sparkplug_publisher_t* pub, const char* host_id,
                                            uint64_t timestamp);

/* ============================================================================
 * Host Application API
 * ========================================================================= */

/** @brief Opaque handle to a Sparkplug Host Application. */
typedef struct sparkplug_host_application sparkplug_host_application_t;

/**
 * @brief Creates a new Sparkplug Host Application.
 *
 * Host Applications have different behavior than Edge Nodes:
 * - Publish STATE messages (JSON, not protobuf) to indicate online/offline status
 * - Publish NCMD/DCMD commands to control Edge Nodes and Devices (group_id specified per command)
 * - Do NOT publish NBIRTH/NDATA/NDEATH (those are for Edge Nodes only)
 *
 * @param broker_url MQTT broker URL (e.g., "tcp://localhost:1883")
 * @param client_id Unique MQTT client identifier
 * @param host_id Host Application identifier (for STATE messages)
 *
 * @return Host Application handle on success, NULL on failure
 *
 * @note Caller must call sparkplug_host_application_destroy() to free resources.
 */
sparkplug_host_application_t* sparkplug_host_application_create(const char* broker_url,
                                                                const char* client_id,
                                                                const char* host_id);

/**
 * @brief Destroys a Host Application and frees all resources.
 *
 * @param host Host Application handle (may be NULL)
 */
void sparkplug_host_application_destroy(sparkplug_host_application_t* host);

/**
 * @brief Sets MQTT username and password for authentication.
 *
 * @param host Host Application handle
 * @param username MQTT username (may be NULL to unset)
 * @param password MQTT password (may be NULL to unset)
 * @return 0 on success, -1 on failure
 *
 * @note Must be called before sparkplug_host_application_connect().
 */
int sparkplug_host_application_set_credentials(sparkplug_host_application_t* host,
                                               const char* username, const char* password);

/**
 * @brief Connects the Host Application to the MQTT broker.
 *
 * Unlike Edge Nodes, this does NOT automatically publish any messages.
 * Call sparkplug_host_application_publish_state_birth() after connecting.
 *
 * @param host Host Application handle
 * @return 0 on success, -1 on failure
 */
int sparkplug_host_application_connect(sparkplug_host_application_t* host);

/**
 * @brief Disconnects the Host Application from the MQTT broker.
 *
 * @param host Host Application handle
 * @return 0 on success, -1 on failure
 *
 * @note Call sparkplug_host_application_publish_state_death() BEFORE disconnect()
 *       to properly signal offline status.
 */
int sparkplug_host_application_disconnect(sparkplug_host_application_t* host);

/**
 * @brief Publishes a STATE birth message to indicate Host Application is online.
 *
 * @param host Host Application handle
 * @param timestamp UTC milliseconds since epoch
 *
 * @return 0 on success, -1 on failure
 *
 * @note Topic: STATE/<host_id>
 * @note Payload: JSON {"online": true, "timestamp": <timestamp>}
 * @note Published with QoS=1, Retain=true
 */
int sparkplug_host_application_publish_state_birth(sparkplug_host_application_t* host,
                                                   uint64_t timestamp);

/**
 * @brief Publishes a STATE death message to indicate Host Application is offline.
 *
 * @param host Host Application handle
 * @param timestamp UTC milliseconds since epoch
 *
 * @return 0 on success, -1 on failure
 *
 * @note Topic: STATE/<host_id>
 * @note Payload: JSON {"online": false, "timestamp": <timestamp>}
 * @note Published with QoS=1, Retain=true
 */
int sparkplug_host_application_publish_state_death(sparkplug_host_application_t* host,
                                                   uint64_t timestamp);

/**
 * @brief Publishes an NCMD (Node Command) message to an Edge Node.
 *
 * @param host Host Application handle
 * @param group_id Sparkplug group ID containing the target Edge Node
 * @param target_edge_node_id Target Edge Node identifier
 * @param payload_data Serialized Sparkplug protobuf payload
 * @param payload_len Length of payload data in bytes
 *
 * @return 0 on success, -1 on failure
 */
int sparkplug_host_application_publish_node_command(sparkplug_host_application_t* host,
                                                    const char* group_id,
                                                    const char* target_edge_node_id,
                                                    const uint8_t* payload_data,
                                                    size_t payload_len);

/**
 * @brief Publishes a DCMD (Device Command) message to a device on an Edge Node.
 *
 * @param host Host Application handle
 * @param group_id Sparkplug group ID containing the target Edge Node
 * @param target_edge_node_id Target Edge Node identifier
 * @param target_device_id Target device identifier
 * @param payload_data Serialized Sparkplug protobuf payload
 * @param payload_len Length of payload data in bytes
 *
 * @return 0 on success, -1 on failure
 */
int sparkplug_host_application_publish_device_command(
    sparkplug_host_application_t* host, const char* group_id, const char* target_edge_node_id,
    const char* target_device_id, const uint8_t* payload_data, size_t payload_len);

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
 * @brief Sets MQTT username and password for authentication.
 *
 * @param sub Subscriber handle
 * @param username MQTT username (may be NULL to unset)
 * @param password MQTT password (may be NULL to unset)
 * @return 0 on success, -1 on failure
 *
 * @note Must be called before sparkplug_subscriber_connect().
 */
int sparkplug_subscriber_set_credentials(sparkplug_subscriber_t* sub, const char* username,
                                         const char* password);

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
 * @brief Subscribes to all messages for an additional group.
 *
 * @param sub Subscriber handle
 * @param group_id Group ID to subscribe to
 *
 * @return 0 on success, -1 on failure
 *
 * @note Allows subscribing to multiple groups on a single MQTT connection.
 */
int sparkplug_subscriber_subscribe_group(sparkplug_subscriber_t* sub, const char* group_id);

/**
 * @brief Subscribes to STATE messages from a primary application.
 *
 * @param sub Subscriber handle
 * @param host_id Primary application host identifier
 *
 * @return 0 on success, -1 on failure
 */
int sparkplug_subscriber_subscribe_state(sparkplug_subscriber_t* sub, const char* host_id);

/**
 * @brief Sets a log callback for receiving library diagnostic messages.
 *
 * @param sub Subscriber handle
 * @param callback Callback function to invoke for log messages
 * @param user_data User-provided context pointer passed to callback
 *
 * @note Set callback to NULL to disable logging (zero overhead).
 * @note The callback may be invoked from any thread.
 * @note Keep callback execution fast to avoid blocking internal operations.
 *
 * @par Example Usage
 * @code
 * void my_log_fn(int level, const char* msg, size_t len, void* ud) {
 *     if (level >= SPARKPLUG_LOG_WARN) {
 *         fprintf(stderr, "[WARN] %.*s\n", (int)len, msg);
 *     }
 * }
 * sparkplug_subscriber_set_log_callback(sub, my_log_fn, NULL);
 * @endcode
 */
void sparkplug_subscriber_set_log_callback(sparkplug_subscriber_t* sub,
                                           sparkplug_log_callback_t callback, void* user_data);

/**
 * @brief Sets a callback for receiving command messages (NCMD/DCMD).
 *
 * @param sub Subscriber handle
 * @param callback Callback function to invoke for command messages
 * @param user_data User-provided context pointer passed to callback
 *
 * @note This callback is invoked in addition to the general message callback.
 * @note Set callback to NULL to disable command callback.
 */
void sparkplug_subscriber_set_command_callback(sparkplug_subscriber_t* sub,
                                               sparkplug_command_callback_t callback,
                                               void* user_data);

/**
 * @brief Resolves a metric alias to its name for a specific node or device.
 *
 * Looks up the metric name that corresponds to the given alias, based on
 * the alias mappings captured from NBIRTH (node metrics) or DBIRTH (device metrics).
 *
 * @param sub Subscriber handle
 * @param group_id The group ID
 * @param edge_node_id The edge node ID
 * @param device_id The device ID (NULL or empty string for node-level metrics)
 * @param alias The metric alias to resolve
 * @param name_buffer Buffer to store the metric name (if found)
 * @param buffer_size Size of the name buffer
 *
 * @return Number of bytes written to name_buffer (including null terminator) on success,
 *         0 if alias not found or node/device not seen yet,
 *         -1 on error (e.g., buffer too small)
 *
 * @note The metric name is null-terminated in the buffer.
 * @note Returns 0 if the node/device hasn't sent a birth message yet.
 *
 * @par Example Usage
 * @code
 * char name[256];
 * int result = sparkplug_subscriber_get_metric_name(
 *     sub, "Energy", "Gateway01", "Sensor01", 1, name, sizeof(name));
 * if (result > 0) {
 *   printf("Alias 1 = %s\n", name);
 * }
 * @endcode
 */
int sparkplug_subscriber_get_metric_name(sparkplug_subscriber_t* sub, const char* group_id,
                                         const char* edge_node_id, const char* device_id,
                                         uint64_t alias, char* name_buffer, size_t buffer_size);

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

/* ============================================================================
 * Payload Parsing and Reading API
 * ========================================================================= */

/**
 * @brief Parses a Sparkplug payload from binary protobuf format.
 *
 * @param data Binary protobuf data
 * @param data_len Length of data in bytes
 *
 * @return Payload handle on success, NULL on parse failure
 *
 * @note Caller must call sparkplug_payload_destroy() to free resources.
 * @note The returned payload can be used with all sparkplug_payload_get_* functions.
 */
sparkplug_payload_t* sparkplug_payload_parse(const uint8_t* data, size_t data_len);

/**
 * @brief Gets the payload-level timestamp.
 *
 * @param payload Payload handle
 * @param out_timestamp Pointer to receive timestamp value
 *
 * @return true if timestamp is present, false otherwise
 */
bool sparkplug_payload_get_timestamp(const sparkplug_payload_t* payload, uint64_t* out_timestamp);

/**
 * @brief Gets the payload-level sequence number.
 *
 * @param payload Payload handle
 * @param out_seq Pointer to receive sequence value
 *
 * @return true if sequence is present, false otherwise
 */
bool sparkplug_payload_get_seq(const sparkplug_payload_t* payload, uint64_t* out_seq);

/**
 * @brief Gets the payload UUID.
 *
 * @param payload Payload handle
 *
 * @return UUID string (owned by payload, valid until sparkplug_payload_destroy()), or NULL if not
 * present
 */
const char* sparkplug_payload_get_uuid(const sparkplug_payload_t* payload);

/**
 * @brief Gets the number of metrics in the payload.
 *
 * @param payload Payload handle
 *
 * @return Number of metrics (0 if payload is NULL)
 */
size_t sparkplug_payload_get_metric_count(const sparkplug_payload_t* payload);

/**
 * @brief Sparkplug data types enum.
 */
typedef enum {
  SPARKPLUG_DATA_TYPE_UNKNOWN = 0,
  SPARKPLUG_DATA_TYPE_INT8 = 1,
  SPARKPLUG_DATA_TYPE_INT16 = 2,
  SPARKPLUG_DATA_TYPE_INT32 = 3,
  SPARKPLUG_DATA_TYPE_INT64 = 4,
  SPARKPLUG_DATA_TYPE_UINT8 = 5,
  SPARKPLUG_DATA_TYPE_UINT16 = 6,
  SPARKPLUG_DATA_TYPE_UINT32 = 7,
  SPARKPLUG_DATA_TYPE_UINT64 = 8,
  SPARKPLUG_DATA_TYPE_FLOAT = 9,
  SPARKPLUG_DATA_TYPE_DOUBLE = 10,
  SPARKPLUG_DATA_TYPE_BOOLEAN = 11,
  SPARKPLUG_DATA_TYPE_STRING = 12,
  SPARKPLUG_DATA_TYPE_DATETIME = 13,
  SPARKPLUG_DATA_TYPE_TEXT = 14,
} sparkplug_data_type_t;

/**
 * @brief Metric value union.
 *
 * @note Check the datatype field to determine which union member is valid.
 */
typedef union {
  int8_t int8_value;
  int16_t int16_value;
  int32_t int32_value;
  int64_t int64_value;
  uint8_t uint8_value;
  uint16_t uint16_value;
  uint32_t uint32_value;
  uint64_t uint64_value;
  float float_value;
  double double_value;
  bool boolean_value;
  const char* string_value; /** Owned by payload, valid until sparkplug_payload_destroy() */
} sparkplug_metric_value_t;

/**
 * @brief Metric information struct.
 *
 * @note String pointers (name, string_value) are owned by the payload and valid until
 * sparkplug_payload_destroy() is called.
 */
typedef struct {
  const char* name;               /** Metric name, or NULL if not present */
  uint64_t alias;                 /** Metric alias */
  uint64_t timestamp;             /** Metric timestamp */
  sparkplug_data_type_t datatype; /** Data type */
  bool has_name;                  /** True if name is present */
  bool has_alias;                 /** True if alias is present */
  bool has_timestamp;             /** True if timestamp is present */
  bool is_null;                   /** True if value is explicitly null */
  sparkplug_metric_value_t value; /** Metric value (only valid if !is_null) */
} sparkplug_metric_t;

/**
 * @brief Gets information about a metric at a specific index.
 *
 * @param payload Payload handle
 * @param index Metric index (0 to metric_count - 1)
 * @param out_metric Pointer to receive metric information
 *
 * @return true on success, false if index is out of bounds or payload is NULL
 *
 * @note The returned pointers in out_metric are valid until sparkplug_payload_destroy() is called.
 *
 * @par Example
 * @code
 * sparkplug_metric_t metric;
 * if (sparkplug_payload_get_metric_at(payload, 0, &metric)) {
 *     printf("Name: %s\n", metric.has_name ? metric.name : "<no name>");
 *     if (!metric.is_null) {
 *         switch (metric.datatype) {
 *             case SPARKPLUG_DATA_TYPE_DOUBLE:
 *                 printf("Value: %f\n", metric.value.double_value);
 *                 break;
 *             case SPARKPLUG_DATA_TYPE_BOOLEAN:
 *                 printf("Value: %s\n", metric.value.boolean_value ? "true" : "false");
 *                 break;
 *         }
 *     }
 * }
 * @endcode
 */
bool sparkplug_payload_get_metric_at(const sparkplug_payload_t* payload, size_t index,
                                     sparkplug_metric_t* out_metric);

#ifdef __cplusplus
}
#endif
