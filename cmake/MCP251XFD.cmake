# MCP251XFD.cmake
# Uses Github Emandhal/MCP251XFD as submodule and loads library as a CMake Lib

set(MCP251XFD_SRC "${CMAKE_SOURCE_DIR}/external/MCP251XFD/MCP251XFD.c")
set(MCP251XFD_HDR "${CMAKE_SOURCE_DIR}/external/MCP251XFD/MCP251XFD.h")


# Create library with single source file
add_library( MCP251XFD ${MCP251XFD_SRC} )

# Include the header file from the source dir
target_include_directories(
    MCP251XFD
    PUBLIC
    ${CMAKE_SOURCE_DIR}/external/MCP251XFD
)

# Sets up a CMake variable to specify where the Conf file lives
set(CONF_MCP251XFD_DIR "" CACHE PATH "Path to Conf_MCP251XFD.h")

#Will error out unless `CONF_MCP251XFD_DIR` is set
if (NOT CONF_MCP251XFD_DIR)
    message(FATAL_ERROR "You must set CONF_MCP251XFD_DIR to the directory containing Conf_MCP251XFD.h")
endif()

find_path(CONF_MCP251XFD_DIR Conf_MCP251XFD.h PATHS ${CONF_MCP251XFD_DIR} REQUIRED)
target_include_directories(MCP251XFD PUBLIC ${CONF_MCP251XFD_DIR})

set_target_properties(MCP251XFD PROPERTIES
    PUBLIC_HEADER "${MCP251XFD_HDR}"
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/external/MCP251XFD
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/external/MCP251XFD
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/external/MCP251XFD
)