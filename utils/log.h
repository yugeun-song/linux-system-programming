#ifndef UTILS_LOG_H
#define UTILS_LOG_H

enum print_padding { PRINT_PADDING_OFF, PRINT_PADDING_ON };

#define PRINT_EMIT(level, errnum, padding, ...)                                                    \
    log_emit((level), __FILE__, __LINE__, __func__, (errnum), (padding), __VA_ARGS__)

#define PRINT_INFO(...)           PRINT_EMIT("INFO", 0, PRINT_PADDING_ON, __VA_ARGS__)
#define PRINT_WARN(...)           PRINT_EMIT("WARN", 0, PRINT_PADDING_ON, __VA_ARGS__)
#define PRINT_ERR(...)            PRINT_EMIT("ERR", 0, PRINT_PADDING_ON, __VA_ARGS__)
#define PRINT_PWARN(errnum, ...)  PRINT_EMIT("WARN", (errnum), PRINT_PADDING_ON, __VA_ARGS__)
#define PRINT_PERROR(errnum, ...) PRINT_EMIT("ERR", (errnum), PRINT_PADDING_ON, __VA_ARGS__)

#define PRINT_INFO_NO_PADDING(...) PRINT_EMIT("INFO", 0, PRINT_PADDING_OFF, __VA_ARGS__)
#define PRINT_WARN_NO_PADDING(...) PRINT_EMIT("WARN", 0, PRINT_PADDING_OFF, __VA_ARGS__)
#define PRINT_ERR_NO_PADDING(...)  PRINT_EMIT("ERR", 0, PRINT_PADDING_OFF, __VA_ARGS__)
#define PRINT_PWARN_NO_PADDING(errnum, ...)                                                        \
    PRINT_EMIT("WARN", (errnum), PRINT_PADDING_OFF, __VA_ARGS__)
#define PRINT_PERROR_NO_PADDING(errnum, ...)                                                       \
    PRINT_EMIT("ERR", (errnum), PRINT_PADDING_OFF, __VA_ARGS__)

__attribute__((format(printf, 7, 8), cold)) void log_emit(const char *level, const char *file,
                                                          int line, const char *func, int errnum,
                                                          enum print_padding padding, const char *fmt, ...);

#endif
