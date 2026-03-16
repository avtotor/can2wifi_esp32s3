#ifndef LOGGER_IDF_H_
#define LOGGER_IDF_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
    LOG_OFF = 4
} LoggerLevel;

void Logger_setLevel(LoggerLevel l);
void Logger_debug(const char *msg, ...);
void Logger_info(const char *msg, ...);
void Logger_warn(const char *msg, ...);
void Logger_error(const char *msg, ...);
void Logger_console(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
