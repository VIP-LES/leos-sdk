# libcanard Driver
# Adapts external/libcanard to interface with the Raspberry Pi Pico and MCP2518FD.

cmake_minimum_required(VERSION 3.15)

# Bring in exernal libraries

add_library(leos_canbus)

target_include_directories(leos_canbus
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(leos_canbus
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(leos_canbus
    PUBLIC
    libcanard
)
target_link_libraries(leos_canbus
    PRIVATE
    o1heap
    pico_stdlib
)


# MCP backend

add_library(leos_canbus_mcp251xfd mcp251xfd/driver.c)
target_include_directories(leos_canbus_mcp251xfd PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/mcp251xfd/include)
target_link_libraries(leos_canbus_mcp251xfd PUBLIC leos_canbus MCP251XFD)
target_link_libraries(leos_canbus_mcp251xfd PRIVATE leos_log)