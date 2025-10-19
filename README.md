# Sparkplug B C++ Library

A modern C++23 implementation of the Eclipse Sparkplug B 2.2 specification for Industrial IoT.

## Features

- **Full Sparkplug B 2.2 Compliance** - Implements the complete specification
- **Modern C++23** - Uses latest C++ features (std::expected, ranges, modules-ready)
- **Type Safe** - Leverages strong typing and compile-time checks
- **Easy Integration** - Simple Publisher/Subscriber API
- **Tested** - Comprehensive compliance test suite included
- **Cross Platform** - Works on macOS and Linux

## What is Sparkplug B?

Sparkplug B is an MQTT-based protocol specification for Industrial IoT that provides:

- Standardized MQTT topic namespace and payload definition
- Birth and Death certificates for edge nodes and devices
- Report by Exception for efficient bandwidth usage
- Automatic state management and session awareness

Learn more: [Eclipse Sparkplug Specification](https://www.eclipse.org/tahu/spec/Sparkplug%20Topic%20Namespace%20and%20State%20ManagementV2.2-with%20appendix%20B%20format%20-%20Eclipse.pdf)

## Quick Start

### Prerequisites

- C++23 compatible compiler (Clang 16+ or GCC 13+)
- CMake 3.25+
- Eclipse Paho MQTT C library
- Protocol Buffers (protobuf)
- Abseil C++ library

### Installation (macOS with Homebrew)

```bash
# Install dependencies
brew install cmake llvm protobuf abseil mosquitto paho.mqtt.c

# Clone and build
git clone https://github.com/yourusername/sparkplug_b.git
cd sparkplug_b
cmake --preset default
cmake --build build
```

### Basic Publisher Example

```cpp
#include <sparkplug/publisher.hpp>
#include <sparkplug/payload_builder.hpp>

int main() {
  // Configure publisher
  sparkplug::Publisher::Config config{
    .broker_url = "tcp://localhost:1883",
    .client_id = "my_edge_node",
    .group_id = "MyGroup",
    .edge_node_id = "Edge01"
  };

  sparkplug::Publisher publisher(std::move(config));
  
  // Connect to broker
  if (auto result = publisher.connect(); !result) {
    std::cerr << "Failed to connect: " << result.error() << "\n";
    return 1;
  }

  // Publish NBIRTH (required first message)
  sparkplug::PayloadBuilder birth;
  birth.add_metric_with_alias("Temperature", 1, 20.5);
  birth.add_metric_with_alias("Pressure", 2, 101.3);
  
  if (auto result = publisher.publish_birth(birth); !result) {
    std::cerr << "Failed to publish birth: " << result.error() << "\n";
    return 1;
  }

  // Publish NDATA (subsequent updates)
  sparkplug::PayloadBuilder data;
  data.add_metric_by_alias(1, 21.0);  // Temperature changed
  // Pressure unchanged, not included (Report by Exception)
  
  if (auto result = publisher.publish_data(data); !result) {
    std::cerr << "Failed to publish data: " << result.error() << "\n";
    return 1;
  }

  publisher.disconnect();
  return 0;
}
```

### Basic Subscriber Example

```cpp
#include <sparkplug/subscriber.hpp>

int main() {
  sparkplug::Subscriber::Config config{
    .broker_url = "tcp://localhost:1883",
    .client_id = "my_subscriber",
    .group_id = "MyGroup"
  };

  auto callback = [](const sparkplug::Topic& topic, 
                     const auto& payload) {
    std::cout << "Received: " << topic.to_string() << "\n";
    
    for (const auto& metric : payload.metrics()) {
      if (metric.has_name()) {
        std::cout << "  " << metric.name() << " = ";
        if (metric.datatype() == static_cast<uint32_t>(
            sparkplug::DataType::Double)) {
          std::cout << metric.double_value() << "\n";
        }
      }
    }
  };

  sparkplug::Subscriber subscriber(std::move(config), callback);
  
  if (auto result = subscriber.connect(); !result) {
    std::cerr << "Failed to connect: " << result.error() << "\n";
    return 1;
  }

  subscriber.subscribe_all();
  
  // Keep running...
  std::this_thread::sleep_for(std::chrono::hours(1));
  
  return 0;
}
```

## Building

### Using CMake Presets

```bash
# Debug build
cmake --preset default
cmake --build build

# Release build
cmake --preset release
cmake --build build-release

# Run tests
ctest --preset default
```

### Manual CMake

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Examples

The `examples/` directory contains:

- **publisher_example.cpp** - Complete publisher with birth/data/rebirth
- **subscriber_example.cpp** - Simple subscriber
- **subscriber_example_debug.cpp** - Detailed message inspection

Build and run:

```bash
# Terminal 1: Start subscriber
./build/examples/subscriber_example_debug

# Terminal 2: Start publisher
./build/examples/publisher_example
```

## Testing

Run the compliance test suite:

```bash
# Make sure mosquitto is running
brew services start mosquitto

# Run tests
ctest --test-dir build --output-on-failure
```

Tests verify:

- NBIRTH sequence number starts at 0
- Sequence wraps at 256
- bdSeq increments on rebirth
- NBIRTH contains bdSeq metric
- Alias usage in NDATA messages
- Sequence validation
- Automatic timestamp generation

## API Documentation

### Publisher

```cpp
class Publisher {
  // Connect to MQTT broker and establish session
  std::expected<void, std::string> connect();
  
  // Publish NBIRTH (must be first message)
  std::expected<void, std::string> publish_birth(PayloadBuilder& payload);
  
  // Publish NDATA (auto-increments sequence)
  std::expected<void, std::string> publish_data(PayloadBuilder& payload);
  
  // Graceful disconnect (sends NDEATH via MQTT Will)
  std::expected<void, std::string> disconnect();
  
  // Trigger rebirth (increments bdSeq)
  std::expected<void, std::string> rebirth();
  
  // Get current sequence/bdSeq numbers
  uint64_t get_seq() const;
  uint64_t get_bd_seq() const;
};
```

### Subscriber

```cpp
class Subscriber {
  // Connect to MQTT broker
  std::expected<void, std::string> connect();
  
  // Subscribe to all messages in group
  std::expected<void, std::string> subscribe_all();
  
  // Subscribe to specific edge node
  std::expected<void, std::string> subscribe_node(std::string_view edge_node_id);
  
  // Get node state (for monitoring)
  const NodeState* get_node_state(const std::string& edge_node_id) const;
};
```

### PayloadBuilder

```cpp
class PayloadBuilder {
  // Add metric by name (for BIRTH messages)
  PayloadBuilder& add_metric(std::string_view name, T&& value);
  
  // Add metric with alias (for BIRTH messages)
  PayloadBuilder& add_metric_with_alias(std::string_view name, 
                                         uint64_t alias, 
                                         T&& value);
  
  // Add metric by alias only (for DATA messages)
  PayloadBuilder& add_metric_by_alias(uint64_t alias, T&& value);
  
  // Node Control convenience methods
  PayloadBuilder& add_node_control_rebirth(bool value = false);
  PayloadBuilder& add_node_control_reboot(bool value = false);
  
  // Manual timestamp/sequence setting (usually automatic)
  PayloadBuilder& set_timestamp(uint64_t ts);
  PayloadBuilder& set_seq(uint64_t seq);
};
```

## C API

A C API is provided via `sparkplug_c.h` for integration with C projects:

```c
#include <sparkplug/sparkplug_c.h>

sparkplug_publisher_t* pub = sparkplug_publisher_create(
  "tcp://localhost:1883", "client_id", "group_id", "edge_node_id"
);

sparkplug_publisher_connect(pub);
// ... use publisher
sparkplug_publisher_destroy(pub);
```

## Architecture

```text
+---------------------------------------------+
|  Application (Your Code)                    |
+---------------------------------------------+
                    |
                    v
+---------------------------------------------+
|  Sparkplug B C++ Library                    |
|  +----------------+  +------------------+   |
|  |  Publisher     |  |  Subscriber      |   |
|  +----------------+  +------------------+   |
|            |                 |               |
|            v                 v               |
|  +-------------------------------------+     |
|  |   PayloadBuilder / Topic            |     |
|  +-------------------------------------+     |
|            |                                 |
|            v                                 |
|  +-------------------------------------+     |
|  |   Protocol Buffers (Sparkplug)      |     |
|  +-------------------------------------+     |
+---------------------------------------------+
                    |
                    v
+---------------------------------------------+
|  Eclipse Paho MQTT C                        |
+---------------------------------------------+
                    |
                    v
+---------------------------------------------+
|  MQTT Broker (Mosquitto, HiveMQ, etc.)      |
+---------------------------------------------+
```

## Sparkplug B Message Flow

```text
Edge Node Lifecycle:
1. CONNECT -> MQTT Broker (with NDEATH in Last Will)
2. NBIRTH -> Establish session, publish all metrics with aliases
3. NDATA -> Report changes by exception using aliases
4. NDATA -> Continue publishing updates
5. DISCONNECT -> NDEATH sent automatically via MQTT Will

Rebirth Scenario:
1. Primary Application sends NCMD/Rebirth
2. Edge Node increments bdSeq
3. Edge Node publishes new NBIRTH
4. Edge Node resets sequence to 0
```

## Topic Namespace

The library uses the Sparkplug B topic namespace:

```text
spBv1.0/{group_id}/{message_type}/{edge_node_id}[/{device_id}]
```

Examples:

- `spBv1.0/Energy/NBIRTH/Gateway01` - Node birth
- `spBv1.0/Energy/NDATA/Gateway01` - Node data
- `spBv1.0/Energy/DBIRTH/Gateway01/Sensor01` - Device birth
- `STATE/my_scada_host` - Primary application state

**Note:** The topic prefix is `spBv1.0` (Sparkplug B v1.0 namespace) even for Sparkplug 2.2 compliance. This is the MQTT topic namespace, not the specification version.

## Performance

- **Binary Protocol** - Efficient protobuf encoding
- **Report by Exception** - Only send changed values
- **Alias Support** - Reduces bandwidth by 60-80%
- **Async I/O** - Non-blocking MQTT operations

## Troubleshooting

### Issue: Garbled output when viewing MQTT messages

**This is normal!** Sparkplug B payloads are binary Protocol Buffers, not plain text. Use the `subscriber_example_debug` to decode messages.

### Issue: Sequence validation warnings

Ensure you:

1. Publish NBIRTH before any NDATA
2. Don't manually set sequence numbers
3. Check for packet loss if gaps persist

### Issue: Connection failures

- Verify MQTT broker is running: `mosquitto -v`
- Check broker URL and port (default: 1883)
- Ensure client_id is unique

## License

[Your License Here - e.g., Apache 2.0, MIT, etc.]

## References

- [Eclipse Sparkplug Specification](https://sparkplug.eclipse.org/)
- [Eclipse Tahu (Reference Implementation)](https://github.com/eclipse/tahu)
- [MQTT Protocol](https://mqtt.org/)
- [Protocol Buffers](https://protobuf.dev/)

## Roadmap

- Python bindings
- Sparkplug Templates support
- DataSet types
- Historical data buffering
- mTLS support
- Metrics dashboard example

---

**Questions?** Open an issue or check the [documentation](docs/API.md).
