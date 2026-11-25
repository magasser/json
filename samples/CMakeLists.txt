add_executable(test_deserialization test_deserialization.c)

add_executable(test_serialization test_serialization.c)

target_link_libraries(test_deserialization PUBLIC json)
target_link_libraries(test_serialization PUBLIC json)

target_include_directories(test_deserialization PRIVATE ${JSON_INCLUDE_DIR})
target_include_directories(test_serialization PRIVATE ${JSON_INCLUDE_DIR})

configure_file(test.json ${CMAKE_BINARY_DIR}/test.json COPYONLY)
configure_file(test_struct.json ${CMAKE_BINARY_DIR}/test_struct.json COPYONLY)
