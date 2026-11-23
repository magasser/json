cmake_minimum_required(VERSION 4.2)
project(json C)

set(CMAKE_C_STANDARD 11)

option(JSON_BUILD_SAMPLES "Build JSON samples" ON)

set(JSON_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

add_subdirectory(src)

if(JSON_BUILD_SAMPLES)
    add_subdirectory(samples)
endif ()
