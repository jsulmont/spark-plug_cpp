# tests/CMakeLists.txt
enable_testing()

add_executable(test_topic test_topic.cpp)
target_link_libraries(test_topic PRIVATE sparkplug_cpp)
add_test(NAME TopicTest COMMAND test_topic)