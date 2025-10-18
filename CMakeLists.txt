# CMakeLists.txt
cmake_minimum_required(VERSION 3.25)
project(SparkplugB VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_COMPILER /opt/homebrew/opt/llvm/bin/clang++)
set(CMAKE_C_COMPILER /opt/homebrew/opt/llvm/bin/clang)

add_compile_options(-stdlib=libc++)
add_link_options(-stdlib=libc++ -L/opt/homebrew/opt/llvm/lib/c++ -Wl,-rpath,/opt/homebrew/opt/llvm/lib/c++)

add_compile_options(-Wall -Wextra -Wpedantic -Wno-c++98-compat)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_PREFIX_PATH "/opt/homebrew")
set(Protobuf_INCLUDE_DIR "/opt/homebrew/include")
set(Protobuf_PROTOC_EXECUTABLE "/opt/homebrew/bin/protoc")

find_package(Protobuf REQUIRED)
find_package(absl REQUIRED)
find_package(eclipse-paho-mqtt-c REQUIRED)

add_subdirectory(proto)
target_compile_options(sparkplug_proto PRIVATE -w)
add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(tests)