# libcanard.cmake
# adapts OpenCyphal's libcanard to CMake

# Always use PROJECT_SOURCE_DIR so paths are relative to the repo root.
set(LIBCANARD_DIR ${LEOS_SDK_ROOT}/external/libcanard/libcanard)
set(CAVL2_DIR ${LEOS_SDK_ROOT}/external/libcanard/lib/cavl2)

set(LIBCANARD_C ${LIBCANARD_DIR}/canard.c)
set(LIBCANARD_H ${LIBCANARD_DIR}/canard.h)

add_library(cavl2 EXCLUDE_FROM_ALL INTERFACE)
target_include_directories(cavl2 INTERFACE ${CAVL2_DIR})

# Build libcanard
add_library(libcanard EXCLUDE_FROM_ALL ${LIBCANARD_C})
target_link_libraries(libcanard PUBLIC cavl2)

# Export its include directory so anyone linking to it can find canard.h
target_include_directories(libcanard PUBLIC ${LIBCANARD_DIR})

# Put the outputs in external/libcanard in the build dir
set_target_properties(libcanard PROPERTIES
    PUBLIC_HEADER "${LIBCANARD_H}"
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/external/libcanard
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/external/libcanard
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/external/libcanard
)