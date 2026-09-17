#ifndef UTILS_LOG_H
#define UTILS_LOG_H

#define LOG_INFO(...)          log_emit("INFO", __FILE__, __LINE__, __func__, 0, __VA_ARGS__)
#define LOG_WARN(...)          log_emit("WARN", __FILE__, __LINE__, __func__, 0, __VA_ARGS__)
#define LOG_ERR(...)           log_emit("ERR", __FILE__, __LINE__, __func__, 0, __VA_ARGS__)
#define LOG_PWARN(errnum, ...) log_emit("WARN", __FILE__, __LINE__, __func__, (errnum), __VA_ARGS__)
#define LOG_PERROR(errnum, ...) log_emit("ERR", __FILE__, __LINE__, __func__, (errnum), __VA_ARGS__)

__attribute__((format(printf, 6, 7), cold)) void log_emit(const char *level, const char *file, int line,
                                                          const char *func, int errnum, const char *fmt, ...);

#endif
