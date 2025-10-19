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

int main(void) {
  printf("=== C API Unit Tests ===\n\n");

  /* Payload tests (no MQTT broker required) */
  test_payload_create_destroy();
  test_payload_add_metrics();
  test_payload_add_with_alias();
  test_payload_add_by_alias();
  test_payload_timestamp_seq();
  test_payload_empty();

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
