# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

# Add our cmake include folder to the search path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Ensure the compile definition can be read by InteliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# If we are the root project (for compile testing)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

    # Pull in Pico SDK
    include(pico_sdk_import)

    project(leos-sdk C CXX ASM)

    pico_sdk_init()

endif()

# Set up nunavut code generation commands
include(nunavut)

# Include all our sdk modules
add_subdirectory(src)


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


