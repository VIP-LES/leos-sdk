# MCP251XFD Driver
# Adapts the external/MCP251XFD library to the Raspberry Pi Pico platform

cmake_minimum_required(VERSION 3.15)

# Bring in exernal/MCP251XFD library
set(CONF_MCP251XFD_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE STRING "Path for MCP251XFD driver")
include(MCP251XFD)

add_library(leos_mcp251xfd EXCLUDE_FROM_ALL mcp251xfd.c platform.c)

target_include_directories(leos_mcp251xfd
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(leos_mcp251xfd
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(leos_mcp251xfd
    PUBLIC
    hardware_spi
    MCP251XFD
)
target_link_libraries(leos_mcp251xfd
    PRIVATE
    leos_log
    pico_time
    pico_stdlib
    hardware_sync
)

target_compile_definitions(leos_mcp251xfd
    PUBLIC
    USE_ERRORS_STRING
)