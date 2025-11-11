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
    set(PICO_BOARD pimoroni_pico_plus2_rp2350 CACHE STRING "Board type")

    include(pico_sdk_import)


    project(leos-sdk C CXX ASM)

    pico_sdk_init()

    add_executable(main main.c config.c)
    target_include_directories(main PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    target_link_libraries(main PUBLIC pico_stdlib leos_mcp251xfd leos_log leos_cyphal_transport_mcp251xfd leos_cyphal_node)
    pico_enable_stdio_usb(main 1)
    pico_add_extra_outputs(main)


endif()

# External dependencies
include(libcanard)
include(o1heap)
include(nunavut)
include(Pico_BusIO)
include(Adafruit_BMP3XX)
include(Adafruit_LTR390)
include(Adafruit_PM25AQI)
include(Adafruit_TSL2561)

# Include all our sdk modules
add_subdirectory(src)