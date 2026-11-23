if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

set(JSON_SOURCES
        json.c
        json_reader.c
        json_stream.c
        json_types.c)

add_library(json STATIC ${JSON_SOURCES})

target_include_directories(json
        PUBLIC ${JSON_INCLUDE_DIR}
        PRIVATE .)