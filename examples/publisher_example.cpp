// expamples/publisher_example.cpp
#include <iostream>
#include <sparkplug/payload_builder.hpp>
#include <sparkplug/publisher.hpp>
#include <thread>

int main() {
  sparkplug::Publisher::Config config{.broker_url = "tcp://localhost:1883",
                                      .client_id =
                                          "sparkplug_publisher_example",
                                      .group_id = "Energy",
                                      .edge_node_id = "Gateway01"};

  sparkplug::Publisher publisher(std::move(config));

  auto connect_result = publisher.connect();
  if (!connect_result) {
    std::cerr << "Failed to connect: " << connect_result.error() << "\n";
    return 1;
  }

  std::cout << "Connected to broker\n";

  sparkplug::PayloadBuilder birth;
  birth.set_seq(0)
      .add_metric("Node Control/Rebirth", false)
      .add_metric("Node Control/Reboot", false)
      .add_metric("Properties/Hardware", "ARM64")
      .add_metric("Properties/OS", "macOS");

  auto birth_result = publisher.publish_birth(birth);
  if (!birth_result) {
    std::cerr << "Failed to publish birth: " << birth_result.error() << "\n";
    return 1;
  }

  std::cout << "Published NBIRTH\n";

  for (int i = 0; i < 10; ++i) {
    sparkplug::PayloadBuilder data;
    data.add_metric("Temperature", 20.5 + i)
        .add_metric("Voltage", 230.0)
        .add_metric("Active", true);

    auto data_result = publisher.publish_data(data);
    if (!data_result) {
      std::cerr << "Failed to publish data: " << data_result.error() << "\n";
    } else {
      std::cout << "Published NDATA " << i << "\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  auto disconnect_result = publisher.disconnect();
  if (!disconnect_result) {
    std::cerr << "Failed to disconnect: " << disconnect_result.error() << "\n";
  }
  std::cout << "Disconnected\n";

  return 0;
}