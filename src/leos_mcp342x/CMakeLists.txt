cmake_minimum_required(VERSION 3.15)


add_library(leos_mcp342x EXCLUDE_FROM_ALL src/mcp342x.c)

target_include_directories(leos_mcp342x
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(leos_mcp342x
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(leos_mcp342x
    PUBLIC
    hardware_i2c
)
target_link_libraries(leos_mcp342x
    PRIVATE
    leos_log
    pico_time
    pico_stdlib
)