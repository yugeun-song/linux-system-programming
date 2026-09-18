#ifndef UTILS_LOG_H
#define UTILS_LOG_H

enum log_padding { LOG_PADDING_OFF, LOG_PADDING_ON };

#define LOG_EMIT(level, errnum, padding, ...)                                                      \
    log_emit((level), __FILE__, __LINE__, __func__, (errnum), (padding), __VA_ARGS__)

#define LOG_INFO(...)           LOG_EMIT("INFO", 0, LOG_PADDING_ON, __VA_ARGS__)
#define LOG_WARN(...)           LOG_EMIT("WARN", 0, LOG_PADDING_ON, __VA_ARGS__)
#define LOG_ERR(...)            LOG_EMIT("ERR", 0, LOG_PADDING_ON, __VA_ARGS__)
#define LOG_PWARN(errnum, ...)  LOG_EMIT("WARN", (errnum), LOG_PADDING_ON, __VA_ARGS__)
#define LOG_PERROR(errnum, ...) LOG_EMIT("ERR", (errnum), LOG_PADDING_ON, __VA_ARGS__)

#define LOG_INFO_NO_PADDING(...)           LOG_EMIT("INFO", 0, LOG_PADDING_OFF, __VA_ARGS__)
#define LOG_WARN_NO_PADDING(...)           LOG_EMIT("WARN", 0, LOG_PADDING_OFF, __VA_ARGS__)
#define LOG_ERR_NO_PADDING(...)            LOG_EMIT("ERR", 0, LOG_PADDING_OFF, __VA_ARGS__)
#define LOG_PWARN_NO_PADDING(errnum, ...)  LOG_EMIT("WARN", (errnum), LOG_PADDING_OFF, __VA_ARGS__)
#define LOG_PERROR_NO_PADDING(errnum, ...) LOG_EMIT("ERR", (errnum), LOG_PADDING_OFF, __VA_ARGS__)

__attribute__((format(printf, 7, 8), cold)) void log_emit(const char *level, const char *file,
                                                          int line, const char *func, int errnum,
                                                          enum log_padding padding, const char *fmt, ...);

#endif
