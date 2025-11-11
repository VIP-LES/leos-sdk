cmake_minimum_required(VERSION 3.15)

add_library(leos_cyphal_transport transport.c)
target_include_directories(leos_cyphal_transport
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(leos_cyphal_transport
    PRIVATE
    leos_log
)
target_link_libraries(leos_cyphal_transport
    PUBLIC
    leos_cyphal
)


# MCP251XFD CAN-FD Transport
add_library(leos_cyphal_transport_mcp251xfd STATIC mcp251xfd/driver.c)
target_include_directories(leos_cyphal_transport_mcp251xfd PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/mcp251xfd/include
)
target_link_libraries(leos_cyphal_transport_mcp251xfd PRIVATE
    leos_log
    libcanard
)
target_link_libraries(leos_cyphal_transport_mcp251xfd PUBLIC
    MCP251XFD
    leos_cyphal_transport
)