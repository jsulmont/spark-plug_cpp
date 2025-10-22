# Sparkplug B C++ Library

A modern C++23 implementation of the Eclipse Sparkplug B 2.2 specification for Industrial IoT.

**[📚 API Documentation](https://jsulmont.github.io/sparkplug-cpp/)** | [Sparkplug Specification](https://www.eclipse.org/tahu/spec/Sparkplug%20Topic%20Namespace%20and%20State%20ManagementV2.2-with%20appendix%20B%20format%20-%20Eclipse.pdf)

## Features

- **Full Sparkplug B 2.2 Compliance** - Implements the complete specification
- **Modern C++23** - Uses latest C++ features (std::expected, ranges, modules-ready)
- **Type Safe** - Leverages strong typing and compile-time checks
- **TLS/SSL Support** - Secure MQTT connections with optional mutual authentication
- **Easy Integration** - Simple Publisher/Subscriber API
- **Tested** - Comprehensive compliance test suite included
- **Cross Platform** - Works on macOS and Linux

## Documentation

Full API documentation is available online **[here](https://jsulmont.github.io/sparkplug-cpp/)**

The documentation includes complete API reference, examples, and class diagrams.

To regenerate locally:
```bash
cmake --build build --target docs
```

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

### Installation

**macOS (Homebrew):**

```bash
# Install dependencies
brew install cmake llvm protobuf abseil mosquitto libpaho-mqtt

# Clone and build
git clone <repository-url>
cd sparkplug_cpp
cmake --preset default
cmake --build build
```

**Linux (Arch Linux - Recommended):**

```bash
# Install system dependencies
sudo pacman -S \
    base-devel clang cmake ninja \
    protobuf abseil-cpp openssl mosquitto

# Build and install Paho MQTT C (not in official repos)
git clone --depth 1 --branch v1.3.15 https://github.com/eclipse/paho.mqtt.c.git
cd paho.mqtt.c
cmake -Bbuild -H. \
  -DCMAKE_BUILD_TYPE=Release \
  -DPAHO_WITH_SSL=TRUE \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build -j$(nproc)
sudo cmake --install build
cd ..

# Clone and build sparkplug_cpp
git clone <repository-url>
cd sparkplug_cpp
cmake --preset default
cmake --build build -j$(nproc)
```

**Linux (Ubuntu/Debian):**

 **Not currently supported** - Ubuntu 24.04's standard library lacks required C++23 features (`std::ranges::to`). Use Arch Linux instead.

**Note:** This project requires full C++23 support including `std::expected` and `std::ranges::to`. Arch Linux with libc++ provides complete implementations.

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

**C++ Examples:**

- **publisher_example.cpp** - Complete publisher with birth/data/rebirth
- **publisher_dynamic_metrics.cpp** - Adding new metrics at runtime via rebirth
- **subscriber_example.cpp** - Simple subscriber
- **subscriber_example_debug.cpp** - Detailed message inspection

**C API Examples:**

- **publisher_example_c.c** - Publisher using C bindings
- **subscriber_example_c.c** - Subscriber using C bindings

**TLS Examples:**

- **publisher_tls_example.cpp** - Secure publisher with TLS/SSL
- **subscriber_tls_example.cpp** - Secure subscriber with TLS/SSL

Build and run:

```bash
# Terminal 1: Start subscriber
./build/examples/subscriber_example_debug

# Terminal 2: Start publisher
./build/examples/publisher_example

# Or try the dynamic metrics example
./build/examples/publisher_dynamic_metrics
```

## TLS/SSL Support

The library supports secure MQTT connections using TLS/SSL encryption. This includes server authentication and optional mutual TLS (client certificates).

### Basic TLS Configuration

```cpp
#include <sparkplug/publisher.hpp>

sparkplug::Publisher::TlsOptions tls{
    .trust_store = "/path/to/ca.crt",          // CA certificate (required)
    .enable_server_cert_auth = true            // Verify server (default)
};

sparkplug::Publisher::Config config{
    .broker_url = "ssl://localhost:8883",      // Use ssl:// prefix for TLS
    .client_id = "secure_publisher",
    .group_id = "Energy",
    .edge_node_id = "Gateway01",
    .tls = tls                                 // Enable TLS
};

sparkplug::Publisher publisher(std::move(config));
publisher.connect();
```

### Mutual TLS (Client Certificates)

```cpp
sparkplug::Publisher::TlsOptions tls{
    .trust_store = "/path/to/ca.crt",          // CA certificate
    .key_store = "/path/to/client.crt",        // Client certificate
    .private_key = "/path/to/client.key",      // Client private key
    .private_key_password = "",                // Optional key password
    .enable_server_cert_auth = true
};
```

### Setting Up TLS

For detailed instructions on generating certificates, configuring Mosquitto with TLS, and troubleshooting, see **[TLS_SETUP.md](TLS_SETUP.md)**.

Quick setup for development:

```bash
# Generate self-signed certificates (development only)
openssl req -new -x509 -days 365 -extensions v3_ca \
    -keyout ca.key -out ca.crt -subj "/CN=MQTT CA"

openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr -subj "/CN=localhost"
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key \
    -CAcreateserial -out server.crt -days 365

# Configure Mosquitto with TLS (add to mosquitto.conf)
listener 8883
cafile /path/to/ca.crt
certfile /path/to/server.crt
keyfile /path/to/server.key

# Restart Mosquitto
brew services restart mosquitto  # macOS
sudo systemctl restart mosquitto # Linux
```

## Testing

Run the compliance test suite:

```bash
# Make sure mosquitto is running
# macOS:
brew services start mosquitto
# Linux:
sudo systemctl start mosquitto

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

## Code Formatting

This project uses **clang-format** for consistent code style. All code is automatically checked in CI.

```bash
# Format all files
./scripts/format.sh

# Check formatting (CI mode)
./scripts/format.sh --check
```

VSCode is configured to format on save automatically. Run the format script before committing changes.

## Generating Documentation

The API is fully documented with Doxygen comments. To generate HTML documentation:

```bash
# Install doxygen (if not already installed)
# macOS:
brew install doxygen graphviz
# Linux:
sudo apt install doxygen graphviz

# Generate documentation
doxygen Doxyfile

# View documentation
open docs/html/index.html  # macOS
# or
xdg-open docs/html/index.html  # Linux
```

The generated documentation includes:

- Complete API reference for both C++ and C APIs
- Class diagrams and dependency graphs
- Code examples from headers
- Cross-referenced source code

All public APIs are documented with:

- Detailed parameter descriptions
- Return value specifications
- Usage examples
- Notes, warnings, and see-also references

**Tip:** Modern IDEs (VSCode, CLion, etc.) display Doxygen documentation as hover tooltips automatically.

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

### Issue: Sequence validation warnings

Ensure you:

1. Publish NBIRTH before any NDATA
2. Don't manually set sequence numbers
3. Check for packet loss if gaps persist

### Issue: Connection failures

- Verify MQTT broker is running:
  - macOS: `brew services start mosquitto`
  - Linux: `sudo systemctl start mosquitto`
  - Or run directly: `mosquitto -v`
- Check broker URL and port (default: 1883)
- Ensure client_id is unique

## License

Licensed under either of:

- Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
- MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

## References

- [Eclipse Sparkplug Specification](https://sparkplug.eclipse.org/)
- [Eclipse Tahu (Reference Implementation)](https://github.com/eclipse/tahu)
- [MQTT Protocol](https://mqtt.org/)
- [Protocol Buffers](https://protobuf.dev/)

## Roadmap

- mTLS support
- Sparkplug Templates support
- Historical data buffering
- Metrics dashboard example

---

**Questions?** See the API documentation in the header files or open an issue.
