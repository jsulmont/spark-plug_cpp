# SparkplugB C++ Library

Modern C++23 implementation of Eclipse Sparkplug B specification with C bindings for the energy industry.

## Features

- Full Sparkplug B v1.0 specification compliance
- Modern C++23 with `std::expected` error handling and deducing `this`
- Clean C API for maximum compatibility
- Protocol Buffers for efficient serialization
- MQTT v3.1.1 support via Eclipse Paho
- Publisher and Subscriber support

## Requirements

- macOS 11+ (ARM64 or x86_64)
- Clang 17+ (tested with Homebrew LLVM 21.1.3)
- CMake 3.25+

## Installation

```bash
brew install llvm cmake
```

## Building

```bash
cmake --preset default
cmake --build build
```

## Running Tests

```bash
ctest --test-dir build
```

## Quick Start - C++ API

```cpp
#include <sparkplug/publisher.hpp>

sparkplug::Publisher::Config config{
    .broker_url = "tcp://localhost:1883",
    .client_id = "node1",
    .group_id = "Energy",
    .edge_node_id = "Gateway01"
};

sparkplug::Publisher pub(std::move(config));
pub.connect();

sparkplug::PayloadBuilder birth;
birth.set_seq(0)
     .add_metric("Temperature", 25.5)
     .add_metric("Voltage", 230.0);

pub.publish_birth(birth);
```

## Quick Start - C API

```c
#include <sparkplug/sparkplug_c.h>

sparkplug_publisher_t* pub = sparkplug_publisher_create(
    "tcp://localhost:1883",
    "node1",
    "Energy",
    "Gateway01"
);

sparkplug_publisher_connect(pub);

sparkplug_payload_t* payload = sparkplug_payload_create();
sparkplug_payload_set_seq(payload, 0);
sparkplug_payload_add_double(payload, "Temperature", 25.5);

uint8_t buffer[1024];
size_t size = sparkplug_payload_serialize(payload, buffer, sizeof(buffer));

sparkplug_publisher_publish_birth(pub, buffer, size);

sparkplug_payload_destroy(payload);
sparkplug_publisher_destroy(pub);
```

## Project Structure

```
sparkplug-b/
├── include/sparkplug/      # Public headers
├── src/                    # Implementation
├── proto/                  # Protocol Buffers definitions
├── examples/               # Usage examples
├── tests/                  # Unit tests
└── CMakeLists.txt
```

## License

Apache 2.0
