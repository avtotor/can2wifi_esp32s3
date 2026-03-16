#include "logger_idf.h"
#include "config_idf.h"
#include "uart_serial.h"
#include "esp_timer.h"
#include <stdarg.h>
#include <stdio.h>

static LoggerLevel s_level = LOG_INFO;
static uint32_t s_lastLogTime = 0;

static uint32_t get_log_time_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void Logger_setLevel(LoggerLevel l) { s_level = l; }

static void log_msg(LoggerLevel level, const char *format, va_list args) {
    if (level < s_level) return;
    s_lastLogTime = get_log_time_ms();
    char buf[192];
    int n = vsnprintf(buf, sizeof(buf), format, args);
    if (n <= 0) return;
    const char *tag = "?";
    switch (level) {
        case LOG_DEBUG: tag = "DEBUG"; break;
        case LOG_INFO:  tag = "INFO";  break;
        case LOG_WARN:  tag = "WARN";  break;
        case LOG_ERROR: tag = "ERROR"; break;
        default: break;
    }
    uart_serial_printf("%lu - %s: ", (unsigned long)s_lastLogTime, tag);
    uart_serial_write((const uint8_t *)buf, (size_t)n);
    uart_serial_print("\n");
}

void Logger_debug(const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    log_msg(LOG_DEBUG, msg, args);
    va_end(args);
}

void Logger_info(const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    log_msg(LOG_INFO, msg, args);
    va_end(args);
}

void Logger_warn(const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    log_msg(LOG_WARN, msg, args);
    va_end(args);
}

void Logger_error(const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    log_msg(LOG_ERROR, msg, args);
    va_end(args);
}

void Logger_console(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (s_level <= LOG_INFO) {
        char buf[128];
        int n = vsnprintf(buf, sizeof(buf), fmt, args);
        if (n > 0)
            uart_serial_write((const uint8_t *)buf, (size_t)n);
    }
    va_end(args);
}
