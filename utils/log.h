#ifndef HELPER_LOG_H
#define HELPER_LOG_H

#define LOG_INFO(...)       log_emit('I', __FILE__, __LINE__, __func__, 0, __VA_ARGS__)
#define LOG_WARN(...)       log_emit('W', __FILE__, __LINE__, __func__, 0, __VA_ARGS__)
#define LOG_ERR(...)        log_emit('E', __FILE__, __LINE__, __func__, 0, __VA_ARGS__)
#define LOG_PWARN(rc, ...)  log_emit('W', __FILE__, __LINE__, __func__, (rc), __VA_ARGS__)
#define LOG_PERROR(rc, ...) log_emit('E', __FILE__, __LINE__, __func__, (rc), __VA_ARGS__)

__attribute__((format(printf, 6, 7), cold)) void log_emit(char level, const char *file, int line,
                                                          const char *func, int errnum, const char *fmt, ...);

#endif