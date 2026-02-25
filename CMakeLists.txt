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

    project(leos-sdk C CXX ASM)

    pico_sdk_init()

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

# Test target that compiles all excluded-from-all libraries
add_custom_target(test ALL
    DEPENDS
        leos_log
        leos_mcp251xfd
        leos_purpleboard
        leos_cyphal_node
        leos_cyphal_transport
        leos_cyphal_transport_mcp251xfd
)


# cyphal_generate_types(
#     TARGET TEMPLATE_PICO
#     DSDL_DIR ${PROJECT_SOURCE_DIR}/external/public_regulated_data_types/uavcan
# )

# # Generate reg types — reg depends on uavcan, so pass uavcan as lookup
# cyphal_generate_types(
#     TARGET TEMPLATE_PICO
#     DSDL_DIR ${PROJECT_SOURCE_DIR}/external/public_regulated_data_types/reg
#     DSDL_LOOKUP_DIRS ${PROJECT_SOURCE_DIR}/external/public_regulated_data_types/uavcan
# )


