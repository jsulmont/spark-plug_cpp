# CMakeLists.txt
cmake_minimum_required(VERSION 3.25)

# Platform-specific compiler detection (must be before project())
if(APPLE)
    # macOS: Use Homebrew LLVM if not specified
    if(NOT DEFINED CMAKE_CXX_COMPILER)
        set(CMAKE_CXX_COMPILER /opt/homebrew/opt/llvm/bin/clang++)
    endif()
    if(NOT DEFINED CMAKE_C_COMPILER)
        set(CMAKE_C_COMPILER /opt/homebrew/opt/llvm/bin/clang)
    endif()
endif()

project(SparkplugB VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Platform-specific flags
if(APPLE)
    # C++ specific flags for libc++
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>)
    add_link_options($<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>)
    add_link_options($<$<COMPILE_LANGUAGE:CXX>:-L/opt/homebrew/opt/llvm/lib/c++>)
    add_link_options($<$<COMPILE_LANGUAGE:CXX>:-Wl,-rpath,/opt/homebrew/opt/llvm/lib/c++>)

    set(CMAKE_PREFIX_PATH "/opt/homebrew")
    set(Protobuf_INCLUDE_DIR "/opt/homebrew/include")
    set(Protobuf_PROTOC_EXECUTABLE "/opt/homebrew/bin/protoc")
elseif(UNIX)
    # Linux: Use libstdc++ (default) for compatibility with system packages
    # No special flags needed - libstdc++ is the default
endif()

# Common warning flags
add_compile_options(-Wall -Wextra -Wpedantic)

# C++ specific warnings
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-c++98-compat>)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Protobuf REQUIRED)
find_package(absl REQUIRED)
find_package(eclipse-paho-mqtt-c REQUIRED)

add_subdirectory(proto)
target_compile_options(sparkplug_proto PRIVATE -w)
add_subdirectory(src)
add_subdirectory(examples)

enable_testing()
add_subdirectory(tests)