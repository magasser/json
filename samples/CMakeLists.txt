add_executable(test test.c)

target_link_libraries(test PUBLIC json)

target_include_directories(test PRIVATE ${JSON_INCLUDE_DIR})

configure_file(test.json ${CMAKE_BINARY_DIR}/test.json COPYONLY)
configure_file(test_struct.json ${CMAKE_BINARY_DIR}/test_struct.json COPYONLY)
