# MCP251XFD Driver
# Adapts the external/MCP251XFD library to the Raspberry Pi Pico platform

cmake_minimum_required(VERSION 3.15)

add_library(leos_cyphal_node cyphal.c memory.c)

cyphal_generate_types(
    TARGET leos_cyphal_node
    DSDL_DIR ${PROJECT_SOURCE_DIR}/external/public_regulated_data_types/uavcan
)

# # Generate reg types — reg depends on uavcan, so pass uavcan as lookup
cyphal_generate_types(
    TARGET leos_cyphal_node
    DSDL_DIR ${PROJECT_SOURCE_DIR}/external/public_regulated_data_types/reg
    DSDL_LOOKUP_DIRS ${PROJECT_SOURCE_DIR}/external/public_regulated_data_types/uavcan
)

target_include_directories(leos_cyphal_node
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(leos_cyphal_node
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(leos_cyphal_node
    PRIVATE
    leos_log
    leos_canbus
    pico_stdlib
    o1heap
)
target_link_libraries(leos_cyphal_node
    PUBLIC
    libcanard
    pico_time
)