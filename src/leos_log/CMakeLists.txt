add_library(leos_log STATIC EXCLUDE_FROM_ALL log.c)
target_include_directories(leos_log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(leos_log PUBLIC ulog)

if (NOT(CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    target_link_libraries(leos_log PRIVATE pico_stdio pico_time)
    target_compile_definitions(leos_log PRIVATE IS_LINUX=0)
else()
    target_compile_definitions(leos_log PRIVATE IS_LINUX=1)
endif()

target_compile_definitions(leos_log PUBLIC LEOS_LOG_ENABLED ULOG_ENABLED)