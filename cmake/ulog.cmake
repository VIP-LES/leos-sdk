add_subdirectory(
    ${LEOS_SDK_ROOT}/external/ulog/src
    ${CMAKE_BINARY_DIR}/external/ulog
    EXCLUDE_FROM_ALL
)
target_compile_definitions(ulog PUBLIC ULOG_ENABLED)