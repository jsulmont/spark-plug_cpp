# tests/CMakeLists.txt
enable_testing()

add_executable(test_topic test_topic.cpp)
target_link_libraries(test_topic PRIVATE sparkplug_cpp)
add_test(NAME TopicTest COMMAND test_topic)

add_executable(test_compliance test_compliance.cpp)
target_link_libraries(test_compliance PRIVATE sparkplug_cpp)
add_test(NAME ComplianceTest COMMAND test_compliance)