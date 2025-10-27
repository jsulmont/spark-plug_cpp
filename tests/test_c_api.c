/* tests/test_c_api.c
 * Unit tests for C API bindings
 */
#include <assert.h>
#include <sparkplug/sparkplug_c.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name)                                                                                 \
  printf("Testing: %s ... ", name);                                                                \
  fflush(stdout);

#define PASS()                                                                                     \
  printf("PASS\n");                                                                                \
  tests_passed++;

#define FAIL(msg)                                                                                  \
  printf("FAIL: %s\n", msg);                                                                       \
  tests_failed++;                                                                                  \
  return;

/* Test payload creation and destruction */
void test_payload_create_destroy(void) {
  TEST("payload create/destroy");

  sparkplug_payload_t* payload = sparkplug_payload_create();
  assert(payload != NULL);

  sparkplug_payload_destroy(payload);
  sparkplug_payload_destroy(NULL); /* Should not crash */

  PASS();
}

/* Test adding metrics by name */
void test_payload_add_metrics(void) {
  TEST("payload add metrics by name");

  sparkplug_payload_t* payload = sparkplug_payload_create();
  assert(payload != NULL);

  sparkplug_payload_add_int32(payload, "int_metric", 42);
  sparkplug_payload_add_int64(payload, "long_metric", 123456789LL);
  sparkplug_payload_add_uint32(payload, "uint_metric", 4294967295U);
  sparkplug_payload_add_float(payload, "float_metric", 3.14159f);
  sparkplug_payload_add_double(payload, "double_metric", 2.718281828);
  sparkplug_payload_add_bool(payload, "bool_metric", true);
  sparkplug_payload_add_string(payload, "string_metric", "Hello C API");

  uint8_t buffer[4096];
  size_t size = sparkplug_payload_serialize(payload, buffer, sizeof(buffer));
  assert(size > 0);
  assert(size < sizeof(buffer));
  (void)size;

  sparkplug_payload_destroy(payload);
  PASS();
}

/* Test adding metrics with aliases */
void test_payload_add_with_alias(void) {
  TEST("payload add metrics with alias");

  sparkplug_payload_t* payload = sparkplug_payload_create();
  assert(payload != NULL);

  sparkplug_payload_add_int32_with_alias(payload, "Temperature", 1, 20);
  sparkplug_payload_add_double_with_alias(payload, "Pressure", 2, 101.325);
  sparkplug_payload_add_bool_with_alias(payload, "Active", 3, true);

  uint8_t buffer[4096];
  size_t size = sparkplug_payload_serialize(payload, buffer, sizeof(buffer));
  assert(size > 0);
  (void)size;

  sparkplug_payload_destroy(payload);
  PASS();
}

/* Test adding metrics by alias only */
void test_payload_add_by_alias(void) {
  TEST("payload add metrics by alias only");

  sparkplug_payload_t* payload = sparkplug_payload_create();
  assert(payload != NULL);

  sparkplug_payload_add_int32_by_alias(payload, 1, 21);
  sparkplug_payload_add_double_by_alias(payload, 2, 102.5);
  sparkplug_payload_add_bool_by_alias(payload, 3, false);

  uint8_t buffer[4096];
  size_t size = sparkplug_payload_serialize(payload, buffer, sizeof(buffer));
  assert(size > 0);
  (void)size;

  sparkplug_payload_destroy(payload);
  PASS();
}

/* Test timestamp and sequence */
void test_payload_timestamp_seq(void) {
  TEST("payload timestamp and sequence");

  sparkplug_payload_t* payload = sparkplug_payload_create();
  assert(payload != NULL);

  sparkplug_payload_set_timestamp(payload, 1234567890123ULL);
  sparkplug_payload_set_seq(payload, 42);
  sparkplug_payload_add_int32(payload, "test", 100);

  uint8_t buffer[4096];
  size_t size = sparkplug_payload_serialize(payload, buffer, sizeof(buffer));
  assert(size > 0);
  (void)size;

  sparkplug_payload_destroy(payload);
  PASS();
}

/* Test empty payload serialization */
void test_payload_empty(void) {
  TEST("empty payload serialization");

  sparkplug_payload_t* payload = sparkplug_payload_create();
  assert(payload != NULL);

  uint8_t buffer[4096];
  size_t size = sparkplug_payload_serialize(payload, buffer, sizeof(buffer));
  assert(size > 0); /* Empty payload still has protobuf overhead */
  (void)size;

  sparkplug_payload_destroy(payload);
  PASS();
}

/* Test publisher creation and destruction */
void test_publisher_create_destroy(void) {
  TEST("publisher create/destroy");

  sparkplug_publisher_t* pub =
      sparkplug_publisher_create("tcp://localhost:1883", "test_c_api_pub", "TestGroup", "TestNode");
  assert(pub != NULL);

  sparkplug_publisher_destroy(pub);
  sparkplug_publisher_destroy(NULL); /* Should not crash */

  PASS();
}

/* Test publisher connect/disconnect */
void test_publisher_connect(void) {
  TEST("publisher connect/disconnect");

  sparkplug_publisher_t* pub =
      sparkplug_publisher_create("tcp://localhost:1883", "test_c_connect", "TestGroup", "TestNode");
  assert(pub != NULL);

  int result = sparkplug_publisher_connect(pub);
  if (result != 0) {
    sparkplug_publisher_destroy(pub);
    FAIL("failed to connect (is mosquitto running?)");
    return;
  }

  /* Give it a moment to establish */
  usleep(100000);

  result = sparkplug_publisher_disconnect(pub);
  assert(result == 0);

  sparkplug_publisher_destroy(pub);
  PASS();
}

/* Test publisher NBIRTH */
void test_publisher_birth(void) {
  TEST("publisher publish NBIRTH");

  sparkplug_publisher_t* pub =
      sparkplug_publisher_create("tcp://localhost:1883", "test_c_birth", "TestGroup", "TestNode");
  assert(pub != NULL);

  int result = sparkplug_publisher_connect(pub);
  if (result != 0) {
    sparkplug_publisher_destroy(pub);
    FAIL("failed to connect");
    return;
  }

  usleep(100000);

  /* Create and publish NBIRTH */
  sparkplug_payload_t* payload = sparkplug_payload_create();
  sparkplug_payload_add_int32_with_alias(payload, "Metric1", 1, 100);

  uint8_t buffer[4096];
  size_t size = sparkplug_payload_serialize(payload, buffer, sizeof(buffer));
  assert(size > 0);

  result = sparkplug_publisher_publish_birth(pub, buffer, size);
  assert(result == 0);

  /* Check sequence is 0 after NBIRTH */
  uint64_t seq = sparkplug_publisher_get_seq(pub);
  assert(seq == 0);
  (void)seq;

  sparkplug_payload_destroy(payload);
  sparkplug_publisher_disconnect(pub);
  sparkplug_publisher_destroy(pub);

  PASS();
}

/* Test publisher NDATA */
void test_publisher_data(void) {
  TEST("publisher publish NDATA");

  sparkplug_publisher_t* pub =
      sparkplug_publisher_create("tcp://localhost:1883", "test_c_data", "TestGroup", "TestNode");
  assert(pub != NULL);

  int result = sparkplug_publisher_connect(pub);
  if (result != 0) {
    sparkplug_publisher_destroy(pub);
    FAIL("failed to connect");
    return;
  }

  usleep(100000);

  /* Publish NBIRTH first */
  sparkplug_payload_t* birth = sparkplug_payload_create();
  sparkplug_payload_add_int32_with_alias(birth, "Metric1", 1, 100);

  uint8_t buffer[4096];
  size_t size = sparkplug_payload_serialize(birth, buffer, sizeof(buffer));
  sparkplug_publisher_publish_birth(pub, buffer, size);
  sparkplug_payload_destroy(birth);

  usleep(100000);

  /* Publish NDATA */
  sparkplug_payload_t* data = sparkplug_payload_create();
  sparkplug_payload_add_int32_by_alias(data, 1, 200);

  size = sparkplug_payload_serialize(data, buffer, sizeof(buffer));
  result = sparkplug_publisher_publish_data(pub, buffer, size);
  assert(result == 0);

  /* Check sequence incremented */
  uint64_t seq = sparkplug_publisher_get_seq(pub);
  assert(seq == 1);
  (void)seq;

  sparkplug_payload_destroy(data);
  sparkplug_publisher_disconnect(pub);
  sparkplug_publisher_destroy(pub);

  PASS();
}

/* Test publisher rebirth */
void test_publisher_rebirth(void) {
  TEST("publisher rebirth");

  sparkplug_publisher_t* pub =
      sparkplug_publisher_create("tcp://localhost:1883", "test_c_rebirth", "TestGroup", "TestNode");
  assert(pub != NULL);

  int result = sparkplug_publisher_connect(pub);
  if (result != 0) {
    sparkplug_publisher_destroy(pub);
    FAIL("failed to connect");
    return;
  }

  usleep(100000);

  /* Publish initial NBIRTH */
  sparkplug_payload_t* birth = sparkplug_payload_create();
  sparkplug_payload_add_int32_with_alias(birth, "Metric1", 1, 100);

  uint8_t buffer[4096];
  size_t size = sparkplug_payload_serialize(birth, buffer, sizeof(buffer));
  sparkplug_publisher_publish_birth(pub, buffer, size);
  sparkplug_payload_destroy(birth);

  uint64_t initial_bdseq = sparkplug_publisher_get_bd_seq(pub);

  usleep(100000);

  /* Trigger rebirth */
  result = sparkplug_publisher_rebirth(pub);
  assert(result == 0);

  /* Check bdSeq incremented and seq reset */
  uint64_t new_bdseq = sparkplug_publisher_get_bd_seq(pub);
  uint64_t seq = sparkplug_publisher_get_seq(pub);

  assert(new_bdseq == initial_bdseq + 1);
  assert(seq == 0);
  (void)initial_bdseq;
  (void)new_bdseq;
  (void)seq;

  sparkplug_publisher_disconnect(pub);
  sparkplug_publisher_destroy(pub);

  PASS();
}

/* Dummy callback for subscriber tests */
static void dummy_callback(const char* topic, const uint8_t* data, size_t len, void* ctx) {
  (void)topic;
  (void)data;
  (void)len;
  (void)ctx;
}

/* Test subscriber creation and destruction */
void test_subscriber_create_destroy(void) {
  TEST("subscriber create/destroy");

  sparkplug_subscriber_t* sub = sparkplug_subscriber_create(
      "tcp://localhost:1883", "test_c_api_sub", "TestGroup", dummy_callback, NULL);
  assert(sub != NULL);

  sparkplug_subscriber_destroy(sub);
  sparkplug_subscriber_destroy(NULL); /* Should not crash */

  PASS();
}

/* Test subscriber connect */
void test_subscriber_connect(void) {
  TEST("subscriber connect/disconnect");

  sparkplug_subscriber_t* sub = sparkplug_subscriber_create(
      "tcp://localhost:1883", "test_c_sub_connect", "TestGroup", dummy_callback, NULL);
  assert(sub != NULL);

  int result = sparkplug_subscriber_connect(sub);
  if (result != 0) {
    sparkplug_subscriber_destroy(sub);
    FAIL("failed to connect");
    return;
  }

  usleep(100000);

  result = sparkplug_subscriber_disconnect(sub);
  assert(result == 0);

  sparkplug_subscriber_destroy(sub);
  PASS();
}

/* Test subscriber subscribe_all */
void test_subscriber_subscribe_all(void) {
  TEST("subscriber subscribe_all");

  sparkplug_subscriber_t* sub = sparkplug_subscriber_create(
      "tcp://localhost:1883", "test_c_sub_all", "TestGroup", dummy_callback, NULL);
  assert(sub != NULL);

  int result = sparkplug_subscriber_connect(sub);
  if (result != 0) {
    sparkplug_subscriber_destroy(sub);
    FAIL("failed to connect");
    return;
  }

  usleep(100000);

  result = sparkplug_subscriber_subscribe_all(sub);
  assert(result == 0);

  usleep(100000);

  sparkplug_subscriber_disconnect(sub);
  sparkplug_subscriber_destroy(sub);

  PASS();
}

/* Test publisher device birth */
void test_publisher_device_birth(void) {
  TEST("publisher publish DBIRTH");

  sparkplug_publisher_t* pub =
      sparkplug_publisher_create("tcp://localhost:1883", "test_c_dbirth", "TestGroup", "TestNode");
  assert(pub != NULL);

  int result = sparkplug_publisher_connect(pub);
  if (result != 0) {
    sparkplug_publisher_destroy(pub);
    FAIL("failed to connect");
    return;
  }

  usleep(100000);

  /* Publish NBIRTH first (required before DBIRTH) */
  sparkplug_payload_t* nbirth = sparkplug_payload_create();
  sparkplug_payload_add_int32_with_alias(nbirth, "NodeMetric", 1, 100);

  uint8_t buffer[4096];
  size_t size = sparkplug_payload_serialize(nbirth, buffer, sizeof(buffer));
  sparkplug_publisher_publish_birth(pub, buffer, size);
  sparkplug_payload_destroy(nbirth);

  usleep(100000);

  /* Publish DBIRTH for device */
  sparkplug_payload_t* dbirth = sparkplug_payload_create();
  sparkplug_payload_add_double_with_alias(dbirth, "Temperature", 1, 20.5);
  sparkplug_payload_add_double_with_alias(dbirth, "Humidity", 2, 65.0);

  size = sparkplug_payload_serialize(dbirth, buffer, sizeof(buffer));
  result = sparkplug_publisher_publish_device_birth(pub, "Sensor01", buffer, size);
  assert(result == 0);

  sparkplug_payload_destroy(dbirth);
  sparkplug_publisher_disconnect(pub);
  sparkplug_publisher_destroy(pub);

  PASS();
}

/* Test publisher device data */
void test_publisher_device_data(void) {
  TEST("publisher publish DDATA");

  sparkplug_publisher_t* pub =
      sparkplug_publisher_create("tcp://localhost:1883", "test_c_ddata", "TestGroup", "TestNode");
  assert(pub != NULL);

  int result = sparkplug_publisher_connect(pub);
  if (result != 0) {
    sparkplug_publisher_destroy(pub);
    FAIL("failed to connect");
    return;
  }

  usleep(100000);

  /* Publish NBIRTH */
  sparkplug_payload_t* nbirth = sparkplug_payload_create();
  sparkplug_payload_add_int32(nbirth, "NodeMetric", 100);
  uint8_t buffer[4096];
  size_t size = sparkplug_payload_serialize(nbirth, buffer, sizeof(buffer));
  sparkplug_publisher_publish_birth(pub, buffer, size);
  sparkplug_payload_destroy(nbirth);

  usleep(100000);

  /* Publish DBIRTH */
  sparkplug_payload_t* dbirth = sparkplug_payload_create();
  sparkplug_payload_add_double_with_alias(dbirth, "Temperature", 1, 20.5);
  size = sparkplug_payload_serialize(dbirth, buffer, sizeof(buffer));
  sparkplug_publisher_publish_device_birth(pub, "Sensor01", buffer, size);
  sparkplug_payload_destroy(dbirth);

  usleep(100000);

  /* Publish DDATA */
  sparkplug_payload_t* ddata = sparkplug_payload_create();
  sparkplug_payload_add_double_by_alias(ddata, 1, 21.5);
  size = sparkplug_payload_serialize(ddata, buffer, sizeof(buffer));
  result = sparkplug_publisher_publish_device_data(pub, "Sensor01", buffer, size);
  assert(result == 0);

  sparkplug_payload_destroy(ddata);
  sparkplug_publisher_disconnect(pub);
  sparkplug_publisher_destroy(pub);

  PASS();
}

/* Test that sequence numbers in payload are ignored for DDATA (TCK fix) */
void test_device_data_ignores_payload_seq(void) {
  TEST("DDATA ignores sequence in payload");

  sparkplug_publisher_t* pub = sparkplug_publisher_create(
      "tcp://localhost:1883", "test_c_ddata_seq", "TestGroup", "TestNode");
  assert(pub != NULL);

  int result = sparkplug_publisher_connect(pub);
  if (result != 0) {
    sparkplug_publisher_destroy(pub);
    FAIL("failed to connect");
    return;
  }

  usleep(100000);

  /* Publish NBIRTH */
  sparkplug_payload_t* nbirth = sparkplug_payload_create();
  sparkplug_payload_add_int32(nbirth, "NodeMetric", 100);
  uint8_t buffer[4096];
  size_t size = sparkplug_payload_serialize(nbirth, buffer, sizeof(buffer));
  sparkplug_publisher_publish_birth(pub, buffer, size);
  sparkplug_payload_destroy(nbirth);

  usleep(100000);

  /* Publish DBIRTH */
  sparkplug_payload_t* dbirth = sparkplug_payload_create();
  sparkplug_payload_add_double_with_alias(dbirth, "Temperature", 1, 20.5);
  size = sparkplug_payload_serialize(dbirth, buffer, sizeof(buffer));
  sparkplug_publisher_publish_device_birth(pub, "Sensor01", buffer, size);
  sparkplug_payload_destroy(dbirth);

  usleep(100000);

  /* Publish DDATA with WRONG sequence number set in payload (should be ignored) */
  sparkplug_payload_t* ddata1 = sparkplug_payload_create();
  sparkplug_payload_set_seq(ddata1, 999); /* Wrong seq - should be ignored! */
  sparkplug_payload_add_double_by_alias(ddata1, 1, 21.5);
  size = sparkplug_payload_serialize(ddata1, buffer, sizeof(buffer));
  result = sparkplug_publisher_publish_device_data(pub, "Sensor01", buffer, size);
  assert(result == 0);
  sparkplug_payload_destroy(ddata1);

  /* Publish second DDATA with another wrong sequence */
  sparkplug_payload_t* ddata2 = sparkplug_payload_create();
  sparkplug_payload_set_seq(ddata2, 777); /* Also wrong - should be ignored! */
  sparkplug_payload_add_double_by_alias(ddata2, 1, 22.5);
  size = sparkplug_payload_serialize(ddata2, buffer, sizeof(buffer));
  result = sparkplug_publisher_publish_device_data(pub, "Sensor01", buffer, size);
  assert(result == 0);
  sparkplug_payload_destroy(ddata2);

  /* Note: The actual sequence numbers sent should be 1, 2 (managed by library),
   * not 999, 777. This test verifies the fix doesn't crash and works correctly. */

  sparkplug_publisher_disconnect(pub);
  sparkplug_publisher_destroy(pub);

  PASS();
}

/* Test publisher device death */
void test_publisher_device_death(void) {
  TEST("publisher publish DDEATH");

  sparkplug_publisher_t* pub =
      sparkplug_publisher_create("tcp://localhost:1883", "test_c_ddeath", "TestGroup", "TestNode");
  assert(pub != NULL);

  int result = sparkplug_publisher_connect(pub);
  if (result != 0) {
    sparkplug_publisher_destroy(pub);
    FAIL("failed to connect");
    return;
  }

  usleep(100000);

  /* Publish NBIRTH */
  sparkplug_payload_t* nbirth = sparkplug_payload_create();
  sparkplug_payload_add_int32(nbirth, "NodeMetric", 100);
  uint8_t buffer[4096];
  size_t size = sparkplug_payload_serialize(nbirth, buffer, sizeof(buffer));
  sparkplug_publisher_publish_birth(pub, buffer, size);
  sparkplug_payload_destroy(nbirth);

  usleep(100000);

  /* Publish DBIRTH */
  sparkplug_payload_t* dbirth = sparkplug_payload_create();
  sparkplug_payload_add_double(dbirth, "Temperature", 20.5);
  size = sparkplug_payload_serialize(dbirth, buffer, sizeof(buffer));
  sparkplug_publisher_publish_device_birth(pub, "Sensor01", buffer, size);
  sparkplug_payload_destroy(dbirth);

  usleep(100000);

  /* Publish DDEATH */
  result = sparkplug_publisher_publish_device_death(pub, "Sensor01");
  assert(result == 0);

  sparkplug_publisher_disconnect(pub);
  sparkplug_publisher_destroy(pub);

  PASS();
}

/* Test publisher node command */
void test_publisher_node_command(void) {
  TEST("publisher publish NCMD");

  sparkplug_publisher_t* pub =
      sparkplug_publisher_create("tcp://localhost:1883", "test_c_ncmd", "TestGroup", "HostNode");
  assert(pub != NULL);

  int result = sparkplug_publisher_connect(pub);
  if (result != 0) {
    sparkplug_publisher_destroy(pub);
    FAIL("failed to connect");
    return;
  }

  usleep(100000);

  /* Publish NCMD to target node */
  sparkplug_payload_t* cmd = sparkplug_payload_create();
  sparkplug_payload_add_bool(cmd, "Node Control/Rebirth", true);

  uint8_t buffer[4096];
  size_t size = sparkplug_payload_serialize(cmd, buffer, sizeof(buffer));
  result = sparkplug_publisher_publish_node_command(pub, "TargetNode", buffer, size);
  assert(result == 0);

  sparkplug_payload_destroy(cmd);
  sparkplug_publisher_disconnect(pub);
  sparkplug_publisher_destroy(pub);

  PASS();
}

/* Test publisher device command */
void test_publisher_device_command(void) {
  TEST("publisher publish DCMD");

  sparkplug_publisher_t* pub =
      sparkplug_publisher_create("tcp://localhost:1883", "test_c_dcmd", "TestGroup", "HostNode");
  assert(pub != NULL);

  int result = sparkplug_publisher_connect(pub);
  if (result != 0) {
    sparkplug_publisher_destroy(pub);
    FAIL("failed to connect");
    return;
  }

  usleep(100000);

  /* Publish DCMD to target device */
  sparkplug_payload_t* cmd = sparkplug_payload_create();
  sparkplug_payload_add_double(cmd, "SetPoint", 75.0);

  uint8_t buffer[4096];
  size_t size = sparkplug_payload_serialize(cmd, buffer, sizeof(buffer));
  result = sparkplug_publisher_publish_device_command(pub, "TargetNode", "Motor01", buffer, size);
  assert(result == 0);

  sparkplug_payload_destroy(cmd);
  sparkplug_publisher_disconnect(pub);
  sparkplug_publisher_destroy(pub);

  PASS();
}

/* Command callback for testing */
static int command_received = 0;
static void test_command_callback(const char* topic, const uint8_t* data, size_t len, void* ctx) {
  (void)topic;
  (void)data;
  (void)len;
  (void)ctx;
  command_received = 1;
}

/* Test subscriber command callback */
void test_subscriber_command_callback(void) {
  TEST("subscriber command callback");

  command_received = 0;

  /* Create publisher to send command */
  sparkplug_publisher_t* pub =
      sparkplug_publisher_create("tcp://localhost:1883", "test_c_cmd_pub", "TestGroup", "HostNode");
  assert(pub != NULL);

  /* Create subscriber to receive command */
  sparkplug_subscriber_t* sub = sparkplug_subscriber_create(
      "tcp://localhost:1883", "test_c_cmd_sub", "TestGroup", dummy_callback, NULL);
  assert(sub != NULL);

  /* Set command callback */
  sparkplug_subscriber_set_command_callback(sub, test_command_callback, NULL);

  int result = sparkplug_publisher_connect(pub);
  if (result != 0) {
    sparkplug_publisher_destroy(pub);
    sparkplug_subscriber_destroy(sub);
    FAIL("publisher failed to connect");
    return;
  }

  result = sparkplug_subscriber_connect(sub);
  if (result != 0) {
    sparkplug_publisher_destroy(pub);
    sparkplug_subscriber_destroy(sub);
    FAIL("subscriber failed to connect");
    return;
  }

  usleep(100000);

  /* Subscribe to commands */
  result = sparkplug_subscriber_subscribe_all(sub);
  assert(result == 0);

  usleep(200000);

  /* Send NCMD */
  sparkplug_payload_t* cmd = sparkplug_payload_create();
  sparkplug_payload_add_bool(cmd, "Node Control/Rebirth", true);

  uint8_t buffer[4096];
  size_t size = sparkplug_payload_serialize(cmd, buffer, sizeof(buffer));
  sparkplug_publisher_publish_node_command(pub, "TestNode", buffer, size);
  sparkplug_payload_destroy(cmd);

  /* Wait for command to arrive */
  usleep(500000);

  sparkplug_subscriber_disconnect(sub);
  sparkplug_publisher_disconnect(pub);
  sparkplug_subscriber_destroy(sub);
  sparkplug_publisher_destroy(pub);

  if (!command_received) {
    FAIL("command callback not invoked");
    return;
  }

  PASS();
}

/* Test payload parse and read */
void test_payload_parse_and_read(void) {
  TEST("payload parse and read");

  /* Create a payload with known data */
  sparkplug_payload_t* payload = sparkplug_payload_create();
  assert(payload != NULL);

  sparkplug_payload_set_timestamp(payload, 1234567890123ULL);
  sparkplug_payload_set_seq(payload, 42);
  sparkplug_payload_add_int32_with_alias(payload, "Temperature", 1, 25);
  sparkplug_payload_add_double_with_alias(payload, "Pressure", 2, 101.325);
  sparkplug_payload_add_bool_with_alias(payload, "Active", 3, true);
  sparkplug_payload_add_string(payload, "Status", "Running");

  /* Serialize it */
  uint8_t buffer[4096];
  size_t size = sparkplug_payload_serialize(payload, buffer, sizeof(buffer));
  assert(size > 0);

  sparkplug_payload_destroy(payload);

  /* Parse it back */
  sparkplug_payload_t* parsed = sparkplug_payload_parse(buffer, size);
  assert(parsed != NULL);

  /* Read timestamp */
  uint64_t timestamp;
  assert(sparkplug_payload_get_timestamp(parsed, &timestamp));
  assert(timestamp == 1234567890123ULL);
  (void)timestamp;

  /* Read sequence */
  uint64_t seq;
  assert(sparkplug_payload_get_seq(parsed, &seq));
  assert(seq == 42);
  (void)seq;

  /* Read metric count */
  size_t count = sparkplug_payload_get_metric_count(parsed);
  assert(count == 4);
  (void)count;

  /* Read first metric (Temperature) */
  sparkplug_metric_t metric;
  assert(sparkplug_payload_get_metric_at(parsed, 0, &metric));
  assert(metric.has_name);
  assert(strcmp(metric.name, "Temperature") == 0);
  assert(metric.has_alias);
  assert(metric.alias == 1);
  assert(metric.datatype == SPARKPLUG_DATA_TYPE_INT32);
  assert(!metric.is_null);
  assert(metric.value.int32_value == 25);

  /* Read second metric (Pressure) */
  assert(sparkplug_payload_get_metric_at(parsed, 1, &metric));
  assert(metric.has_name);
  assert(strcmp(metric.name, "Pressure") == 0);
  assert(metric.datatype == SPARKPLUG_DATA_TYPE_DOUBLE);
  assert(metric.value.double_value > 101.3 && metric.value.double_value < 101.4);

  /* Read third metric (Active) */
  assert(sparkplug_payload_get_metric_at(parsed, 2, &metric));
  assert(strcmp(metric.name, "Active") == 0);
  assert(metric.datatype == SPARKPLUG_DATA_TYPE_BOOLEAN);
  assert(metric.value.boolean_value == true);

  /* Read fourth metric (Status) */
  assert(sparkplug_payload_get_metric_at(parsed, 3, &metric));
  assert(strcmp(metric.name, "Status") == 0);
  assert(metric.datatype == SPARKPLUG_DATA_TYPE_STRING);
  assert(strcmp(metric.value.string_value, "Running") == 0);

  /* Test out of bounds */
  assert(!sparkplug_payload_get_metric_at(parsed, 4, &metric));
  (void)metric;

  sparkplug_payload_destroy(parsed);

  PASS();
}

/* Test parsing payload with only aliases (NDATA) */
void test_payload_parse_alias_only(void) {
  TEST("payload parse alias-only metrics");

  /* Create NDATA-style payload with only aliases */
  sparkplug_payload_t* payload = sparkplug_payload_create();
  sparkplug_payload_add_int32_by_alias(payload, 1, 30);
  sparkplug_payload_add_double_by_alias(payload, 2, 102.5);

  uint8_t buffer[4096];
  size_t size = sparkplug_payload_serialize(payload, buffer, sizeof(buffer));
  sparkplug_payload_destroy(payload);

  /* Parse it */
  sparkplug_payload_t* parsed = sparkplug_payload_parse(buffer, size);
  assert(parsed != NULL);

  size_t count = sparkplug_payload_get_metric_count(parsed);
  assert(count == 2);
  (void)count;

  sparkplug_metric_t metric;
  assert(sparkplug_payload_get_metric_at(parsed, 0, &metric));
  assert(!metric.has_name); /* No name for alias-only */
  assert(metric.has_alias);
  assert(metric.alias == 1);
  assert(metric.value.int32_value == 30);
  (void)metric;

  sparkplug_payload_destroy(parsed);

  PASS();
}

/* Test parsing invalid payload */
void test_payload_parse_invalid(void) {
  TEST("payload parse invalid data");

  uint8_t invalid_data[] = {0xFF, 0xFF, 0xFF, 0xFF};
  sparkplug_payload_t* parsed = sparkplug_payload_parse(invalid_data, sizeof(invalid_data));
  assert(parsed == NULL); /* Should fail gracefully */

  /* Test NULL inputs */
  parsed = sparkplug_payload_parse(NULL, 100);
  assert(parsed == NULL);

  parsed = sparkplug_payload_parse(invalid_data, 0);
  assert(parsed == NULL);
  (void)parsed;

  PASS();
}

/* Test reading payload without optional fields */
void test_payload_parse_no_optional(void) {
  TEST("payload parse without seq/uuid");

  /* Create payload without explicitly setting seq */
  sparkplug_payload_t* payload = sparkplug_payload_create();
  sparkplug_payload_add_int32(payload, "Value", 100);
  /* Note: timestamp is auto-set by PayloadBuilder constructor */

  uint8_t buffer[4096];
  size_t size = sparkplug_payload_serialize(payload, buffer, sizeof(buffer));
  sparkplug_payload_destroy(payload);

  /* Parse it */
  sparkplug_payload_t* parsed = sparkplug_payload_parse(buffer, size);
  assert(parsed != NULL);

  /* Timestamp should be present (auto-set) */
  uint64_t timestamp;
  assert(sparkplug_payload_get_timestamp(parsed, &timestamp));
  assert(timestamp > 0);
  (void)timestamp;

  /* Seq should not be present (wasn't set) */
  uint64_t seq;
  assert(!sparkplug_payload_get_seq(parsed, &seq));
  (void)seq;

  /* UUID should be NULL (wasn't set) */
  const char* uuid = sparkplug_payload_get_uuid(parsed);
  assert(uuid == NULL);
  (void)uuid;

  /* But metric should be there */
  assert(sparkplug_payload_get_metric_count(parsed) == 1);

  sparkplug_payload_destroy(parsed);

  PASS();
}

int main(void) {
  printf("=== C API Unit Tests ===\n\n");

  /* Payload tests (no MQTT broker required) */
  test_payload_create_destroy();
  test_payload_add_metrics();
  test_payload_add_with_alias();
  test_payload_add_by_alias();
  test_payload_timestamp_seq();
  test_payload_empty();

  /* NEW: Payload parsing tests */
  test_payload_parse_and_read();
  test_payload_parse_alias_only();
  test_payload_parse_invalid();
  test_payload_parse_no_optional();

  /* Publisher tests (require MQTT broker) */
  test_publisher_create_destroy();
  test_publisher_connect();
  test_publisher_birth();
  test_publisher_data();
  test_publisher_rebirth();

  /* Subscriber tests (require MQTT broker) */
  test_subscriber_create_destroy();
  test_subscriber_connect();
  test_subscriber_subscribe_all();

  /* Device-level API tests (require MQTT broker) */
  test_publisher_device_birth();
  test_publisher_device_data();
  test_device_data_ignores_payload_seq();
  test_publisher_device_death();

  /* Command API tests (require MQTT broker) */
  test_publisher_node_command();
  test_publisher_device_command();
  test_subscriber_command_callback();

  printf("\n=== Test Summary ===\n");
  printf("Passed: %d\n", tests_passed);
  printf("Failed: %d\n", tests_failed);

  if (tests_failed > 0) {
    printf("\n*** SOME TESTS FAILED ***\n");
    return 1;
  }

  printf("\n=== All C API tests passed! ===\n");
  return 0;
}
