# libdriver SX1262 HAL
# Adapts the external/sx1262 library to the Raspberry Pi Pico platform

cmake_minimum_required(VERSION 3.15)

include(sx1262)

add_library(leos_sx126x EXCLUDE_FROM_ALL sx126x.c platform.c)

target_include_directories(leos_sx126x
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(leos_sx126x
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(leos_sx126x
    PUBLIC
    hardware_spi
    sx1262
)
target_link_libraries(leos_sx126x
    PRIVATE
    leos_log
    pico_time
    pico_stdlib
    hardware_sync
)