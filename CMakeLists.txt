# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(LEOS_SDK_ROOT ${CMAKE_CURRENT_LIST_DIR})

# Add our cmake include folder to the search path
list(APPEND CMAKE_MODULE_PATH ${LEOS_SDK_ROOT}/cmake)

# Ensure the compile definition can be read by InteliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# If we are the root project (for compile testing)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

    # Pull in Pico SDK
    include(pico_sdk_import)

    project(leos-sdk LANGUAGES C CXX ASM)

    pico_sdk_init()

endif()

add_subdirectory(src)

# Test target that compiles all excluded-from-all libraries
add_custom_target(test
    DEPENDS
        leos_log
        leos_mcp251xfd
        leos_mcp342x
        leos_purpleboard
        leos_cyphal_node
        leos_cyphal_transport
        leos_cyphal_transport_mcp251xfd
)
