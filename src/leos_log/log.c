#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ulog.h"

#if IS_LINUX
    #include <time.h>
#else
    #include "pico/stdio.h"
    #include "pico/time.h"
#endif

#define COL_RESET "\x1b[0m"
#define COL_TRACE "\x1b[35m" // purple
#define COL_DEBUG "\x1b[34m" // blue
#define COL_INFO "\x1b[37m"  // white
#define COL_WARN "\x1b[33m"  // yellow
#define COL_ERROR "\x1b[31m" // red

static uint32_t platform_time_ms(void) {
    #if IS_LINUX
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    #else
        return to_ms_since_boot(get_absolute_time());
    #endif
}

void console_logger(ulog_level_t severity, char *msg) {
    const char* color;
    switch (severity) {
        case ULOG_TRACE_LEVEL: 
            color = COL_TRACE;
            break;
        case ULOG_DEBUG_LEVEL: 
            color = COL_DEBUG;
            break;
        case ULOG_INFO_LEVEL:
            color = COL_INFO;
            break;
        case ULOG_WARNING_LEVEL:
            color = COL_WARN;
            break;
        case ULOG_ERROR_LEVEL:
            color = COL_ERROR; 
            break;
        case ULOG_CRITICAL_LEVEL: 
            color = COL_ERROR;
            break;
        default:
            color = COL_RESET; 
            break;
    }
    const char* level_name = ulog_level_name(severity);

    uint32_t ms = platform_time_ms();
    uint32_t sec = ms / 1000;
    uint32_t msec = ms % 1000;

    printf("%s[%s]" COL_RESET " %lu.%03lu: ", color, level_name, sec, msec);
    printf("%s\n", msg);
}

bool leos_log_init_console(ulog_level_t level) {
    #if !IS_LINUX
        if (!stdio_init_all())
            return false;
    #endif
    ulog_init();
    ulog_err_t result = ulog_subscribe(console_logger, level);
    return (result == ULOG_ERR_NONE);
}