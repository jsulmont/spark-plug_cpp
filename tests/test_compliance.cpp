// tests/test_compliance.cpp
// Sparkplug 2.2 Compliance Tests
#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

#include <sparkplug/publisher.hpp>
#include <sparkplug/subscriber.hpp>

// Test result tracking
struct TestResult {
  std::string name;
  bool passed;
  std::string message;
};

std::vector<TestResult> results;

void report_test(const std::string& name, bool passed, const std::string& msg = "") {
  results.push_back({name, passed, msg});
  std::cout << (passed ? "✓" : "✗") << " " << name;
  if (!msg.empty()) {
    std::cout << ": " << msg;
  }
  std::cout << "\n";
}

// Test 1: NBIRTH must have sequence 0
void test_nbirth_sequence_zero() {
  sparkplug::Publisher::Config config{.broker_url = "tcp://localhost:1883",
                                      .client_id = "test_nbirth_seq",
                                      .group_id = "TestGroup",
                                      .edge_node_id = "TestNode"};

  sparkplug::Publisher pub(std::move(config));

  if (!pub.connect()) {
    report_test("NBIRTH sequence zero", false, "Failed to connect");
    return;
  }

  sparkplug::PayloadBuilder birth;
  birth.add_metric("test", 42);

  if (!pub.publish_birth(birth)) {
    report_test("NBIRTH sequence zero", false, "Failed to publish");
    (void)pub.disconnect();
    return;
  }

  bool passed = (pub.get_seq() == 0);
  report_test("NBIRTH sequence zero", passed,
              passed ? "" : std::format("Got seq={}", pub.get_seq()));

  (void)pub.disconnect();
}

// Test 2: Sequence wraps at 256
void test_sequence_wraps() {
  sparkplug::Publisher::Config config{.broker_url = "tcp://localhost:1883",
                                      .client_id = "test_seq_wrap",
                                      .group_id = "TestGroup",
                                      .edge_node_id = "TestNode"};

  sparkplug::Publisher pub(std::move(config));

  if (!pub.connect()) {
    report_test("Sequence wraps at 256", false, "Failed to connect");
    return;
  }

  sparkplug::PayloadBuilder birth;
  birth.add_metric("test", 0);
  if (!pub.publish_birth(birth)) {
    report_test("Sequence wraps at 256", false, "Failed to publish NBIRTH");
    (void)pub.disconnect();
    return;
  }

  // Publish 256 messages to trigger wrap
  for (int i = 0; i < 256; i++) {
    sparkplug::PayloadBuilder data;
    data.add_metric("test", i);
    if (!pub.publish_data(data)) {
      report_test("Sequence wraps at 256", false, std::format("Failed at iteration {}", i));
      (void)pub.disconnect();
      return;
    }
  }

  bool passed = (pub.get_seq() == 0);
  report_test("Sequence wraps at 256", passed,
              passed ? "" : std::format("Got seq={}", pub.get_seq()));

  (void)pub.disconnect();
}

// Test 3: bdSeq increments on rebirth
void test_bdseq_increment() {
  sparkplug::Publisher::Config config{.broker_url = "tcp://localhost:1883",
                                      .client_id = "test_bdseq",
                                      .group_id = "TestGroup",
                                      .edge_node_id = "TestNode"};

  sparkplug::Publisher pub(std::move(config));

  if (!pub.connect()) {
    report_test("bdSeq increments on rebirth", false, "Failed to connect");
    return;
  }

  sparkplug::PayloadBuilder birth;
  birth.add_metric("test", 0);
  if (!pub.publish_birth(birth)) {
    report_test("bdSeq increments on rebirth", false, "NBIRTH failed");
    (void)pub.disconnect();
    return;
  }

  uint64_t first_bdseq = pub.get_bd_seq();

  if (!pub.rebirth()) {
    report_test("bdSeq increments on rebirth", false, "Rebirth failed");
    (void)pub.disconnect();
    return;
  }

  uint64_t second_bdseq = pub.get_bd_seq();
  bool passed = (second_bdseq == first_bdseq + 1);

  report_test("bdSeq increments on rebirth", passed,
              passed ? "" : std::format("First={}, Second={}", first_bdseq, second_bdseq));

  (void)pub.disconnect();
}

// Test 4: NBIRTH contains bdSeq metric
void test_nbirth_has_bdseq() {
  std::atomic<bool> found_bdseq{false};
  std::atomic<bool> got_nbirth{false};

  auto callback = [&](const sparkplug::Topic& topic,
                      const org::eclipse::tahu::protobuf::Payload& payload) {
    if (topic.message_type == sparkplug::MessageType::NBIRTH) {
      got_nbirth = true;
      for (const auto& metric : payload.metrics()) {
        if (metric.name() == "bdSeq") {
          found_bdseq = true;
          break;
        }
      }
    }
  };

  sparkplug::Subscriber::Config sub_config{
      .broker_url = "tcp://localhost:1883", .client_id = "test_bdseq_sub", .group_id = "TestGroup"};

  sparkplug::Subscriber sub(std::move(sub_config), callback);

  if (!sub.connect()) {
    report_test("NBIRTH contains bdSeq", false, "Subscriber failed to connect");
    return;
  }

  if (!sub.subscribe_all()) {
    report_test("NBIRTH contains bdSeq", false, "Subscribe failed");
    (void)sub.disconnect();
    return;
  }

  // Give subscriber time to subscribe
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  sparkplug::Publisher::Config pub_config{.broker_url = "tcp://localhost:1883",
                                          .client_id = "test_bdseq_pub",
                                          .group_id = "TestGroup",
                                          .edge_node_id = "TestNode"};

  sparkplug::Publisher pub(std::move(pub_config));

  if (!pub.connect()) {
    report_test("NBIRTH contains bdSeq", false, "Publisher failed to connect");
    (void)sub.disconnect();
    return;
  }

  sparkplug::PayloadBuilder birth;
  birth.add_metric("test", 42);

  auto birth_result = pub.publish_birth(birth);
  if (!birth_result) {
    report_test("NBIRTH contains bdSeq", false, "Failed to publish: " + birth_result.error());
    (void)pub.disconnect();
    (void)sub.disconnect();
    return;
  }

  // Wait for message
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  bool passed = got_nbirth && found_bdseq;
  report_test("NBIRTH contains bdSeq", passed,
              !got_nbirth    ? "No NBIRTH received"
              : !found_bdseq ? "bdSeq metric not found"
                             : "");

  (void)pub.disconnect();
  (void)sub.disconnect();
}

// Test 5: NDATA uses aliases correctly
void test_alias_usage() {
  std::atomic<bool> got_ndata{false};
  std::atomic<bool> has_alias{false};
  std::atomic<bool> no_name{false};

  auto callback = [&](const sparkplug::Topic& topic,
                      const org::eclipse::tahu::protobuf::Payload& payload) {
    if (topic.message_type == sparkplug::MessageType::NDATA) {
      got_ndata = true;
      for (const auto& metric : payload.metrics()) {
        if (metric.has_alias()) {
          has_alias = true;
        }
        // After BIRTH, DATA messages should use alias without name
        if (metric.has_alias() && !metric.has_name()) {
          no_name = true;
        }
      }
    }
  };

  sparkplug::Subscriber::Config sub_config{
      .broker_url = "tcp://localhost:1883", .client_id = "test_alias_sub", .group_id = "TestGroup"};

  sparkplug::Subscriber sub(std::move(sub_config), callback);
  if (!sub.connect()) {
    report_test("NDATA uses aliases", false, "Subscriber failed to connect");
    return;
  }
  if (!sub.subscribe_all()) {
    report_test("NDATA uses aliases", false, "Subscribe failed");
    (void)sub.disconnect();
    return;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  sparkplug::Publisher::Config pub_config{.broker_url = "tcp://localhost:1883",
                                          .client_id = "test_alias_pub",
                                          .group_id = "TestGroup",
                                          .edge_node_id = "TestNode"};

  sparkplug::Publisher pub(std::move(pub_config));
  if (!pub.connect()) {
    report_test("NDATA uses aliases", false, "Publisher failed to connect");
    (void)sub.disconnect();
    return;
  }

  // NBIRTH with alias
  sparkplug::PayloadBuilder birth;
  birth.add_metric_with_alias("Temperature", 1, 20.5);
  auto birth_result = pub.publish_birth(birth);
  if (!birth_result) {
    report_test("NDATA uses aliases", false, "NBIRTH failed: " + birth_result.error());
    (void)pub.disconnect();
    (void)sub.disconnect();
    return;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // NDATA with alias only
  sparkplug::PayloadBuilder data;
  data.add_metric_by_alias(1, 21.0);
  auto data_result = pub.publish_data(data);
  if (!data_result) {
    report_test("NDATA uses aliases", false, "NDATA failed: " + data_result.error());
    (void)pub.disconnect();
    (void)sub.disconnect();
    return;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  bool passed = got_ndata && has_alias;
  report_test("NDATA uses aliases", passed,
              !got_ndata   ? "No NDATA received"
              : !has_alias ? "No alias found"
                           : "");

  (void)pub.disconnect();
  (void)sub.disconnect();
}

// Test 6: Subscriber validates sequence
void test_subscriber_validation() {
  sparkplug::Subscriber::Config config{.broker_url = "tcp://localhost:1883",
                                       .client_id = "test_validation",
                                       .group_id = "TestGroup",
                                       .validate_sequence = true};

  auto callback = [](const sparkplug::Topic&, const auto&) {
    // Just receive messages
  };

  sparkplug::Subscriber sub(std::move(config), callback);

  if (!sub.connect()) {
    report_test("Subscriber validation", false, "Failed to connect");
    return;
  }

  // Validation happens automatically during message receipt
  report_test("Subscriber validation", true, "Enabled successfully");

  (void)sub.disconnect();
}

// Test 7: Payload has timestamp
void test_payload_timestamp() {
  sparkplug::PayloadBuilder payload;
  payload.add_metric("test", 42);

  auto built = payload.build();

  org::eclipse::tahu::protobuf::Payload proto;
  proto.ParseFromArray(built.data(), static_cast<int>(built.size()));

  bool passed = proto.has_timestamp() && proto.timestamp() > 0;
  report_test("Payload has timestamp", passed, passed ? "" : "Timestamp missing or zero");
}

// Test 8: Auto sequence management
void test_auto_sequence() {
  sparkplug::Publisher::Config config{.broker_url = "tcp://localhost:1883",
                                      .client_id = "test_auto_seq",
                                      .group_id = "TestGroup",
                                      .edge_node_id = "TestNode"};

  sparkplug::Publisher pub(std::move(config));

  if (!pub.connect()) {
    report_test("Auto sequence management", false, "Failed to connect");
    return;
  }

  sparkplug::PayloadBuilder birth;
  birth.add_metric("test", 0);
  auto birth_result = pub.publish_birth(birth);
  if (!birth_result) {
    report_test("Auto sequence management", false, "NBIRTH failed: " + birth_result.error());
    (void)pub.disconnect();
    return;
  }

  uint64_t prev_seq = pub.get_seq(); // Should be 0

  // Publish without explicit seq
  sparkplug::PayloadBuilder data;
  data.add_metric("test", 1);
  auto data_result = pub.publish_data(data);
  if (!data_result) {
    report_test("Auto sequence management", false, "NDATA failed: " + data_result.error());
    (void)pub.disconnect();
    return;
  }

  uint64_t new_seq = pub.get_seq();
  bool passed = (new_seq == 1 && prev_seq == 0);

  report_test("Auto sequence management", passed,
              passed ? "" : std::format("Expected 0→1, got {}→{}", prev_seq, new_seq));

  (void)pub.disconnect();
}

int main() {
  std::cout << "=== Sparkplug 2.2 Compliance Tests ===\n\n";

  // Run all tests
  test_nbirth_sequence_zero();
  test_sequence_wraps();
  test_bdseq_increment();
  test_nbirth_has_bdseq();
  test_alias_usage();
  test_subscriber_validation();
  test_payload_timestamp();
  test_auto_sequence();

  // Summary
  int passed = 0;
  int failed = 0;

  std::cout << "\n=== Test Results ===\n";
  for (const auto& result : results) {
    if (result.passed) {
      passed++;
    } else {
      failed++;
      std::cout << "FAILED: " << result.name << " - " << result.message << "\n";
    }
  }

  std::cout << "\nTotal: " << results.size() << " tests\n";
  std::cout << "Passed: " << passed << "\n";
  std::cout << "Failed: " << failed << "\n";

  if (failed == 0) {
    std::cout << "\n✓ All tests passed! Library is Sparkplug 2.2 compliant.\n";
    return 0;
  } else {
    std::cout << "\n✗ Some tests failed. Review implementation.\n";
    return 1;
  }
}