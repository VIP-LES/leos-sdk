#pragma once

#include "ulog.h"
#include <stdbool.h>

#ifdef LEOS_LOG_ENABLED
    #define LOG_TRACE(...) ulog_message(ULOG_TRACE_LEVEL, __VA_ARGS__)
    #define LOG_DEBUG(...) ulog_message(ULOG_DEBUG_LEVEL, __VA_ARGS__)
    #define LOG_INFO(...) ulog_message(ULOG_INFO_LEVEL, __VA_ARGS__)
    #define LOG_WARNING(...) ulog_message(ULOG_WARNING_LEVEL, __VA_ARGS__)
    #define LOG_ERROR(...) ulog_message(ULOG_ERROR_LEVEL, __VA_ARGS__)
    #define LOG_CRITICAL(...) ulog_message(ULOG_CRITICAL_LEVEL, __VA_ARGS__)
    #define LOG_ALWAYS(...) ulog_message(ULOG_ALWAYS_LEVEL, __VA_ARGS__)
#else
  #define LOG_TRACE(f, ...) do {} while(0)
  #define LOG_DEBUG(f, ...) do {} while(0)
  #define LOG_INFO(f, ...) do {} while(0)
  #define LOG_WARNING(f, ...) do {} while(0)
  #define LOG_ERROR(f, ...) do {} while(0)
  #define LOG_CRITICAL(f, ...) do {} while(0)
  #define LOG_ALWAYS(f, ...) do {} while(0)
#endif

/**
 * @brief Initialize the console logging backend for the LEOS logging system.
 *
 * This sets up the Pico SDK stdio subsystem (USB CDC or UART depending on
 * build configuration), then registers the console logger as a subscriber
 * to the ulog logging framework.
 *
 * After initialization, log messages will be printed in the form:
 *
 * @code
 * [LEVEL] seconds.milliseconds: message...
 * @endcode
 *
 * Example:
 * @code
 * [INFO]  1.237: System initialized
 * @endcode
 *
 * @param level The minimum log level that the console backend should output.
 *        Messages below this level will be filtered.
 *
 * @return true if initialization succeeds, false if stdio could not be initialized.
 */
bool leos_log_init_console(ulog_level_t level);