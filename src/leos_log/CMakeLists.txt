include(ulog)
add_library(leos_log STATIC EXCLUDE_FROM_ALL log.c)
target_include_directories(leos_log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(leos_log PUBLIC ulog)
target_link_libraries(leos_log PRIVATE pico_stdio pico_time)

target_compile_definitions(leos_log PUBLIC LEOS_LOG_ENABLED ULOG_ENABLED)