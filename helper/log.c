#define _GNU_SOURCE

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "helper/log.h"

static ssize_t write_all(int fd, const void *buf, size_t count)
{
    const char *ptr = buf;
    size_t left = count;

    while (left > 0) {
        ssize_t n = write(fd, ptr, left);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) {
                continue;
            }
            return -1;
        }
        left -= (size_t)n;
        ptr += n;
    }

    return (ssize_t)count;
}

void log_emit(char level, const char *file, int line, const char *func, int errnum, const char *fmt, ...)
{
    int saved_errno = errno;
    char prefix[256];
    char user_msg[512];
    char err_msg[160];
    char errbuf[128];
    char buf[1024];
    struct timespec ts = {
        0,
    };
    struct tm tm = {
        0,
    };
    va_list ap;
    int n;

    prefix[0] = '\0';
    user_msg[0] = '\0';

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm);

    snprintf(prefix, sizeof(prefix), "%02d:%02d:%02d.%03ld [%c] [%d/%d] %s:%d %s(): ", tm.tm_hour,
             tm.tm_min, tm.tm_sec, ts.tv_nsec / 1000000, level, getpid(), gettid(), file, line, func);

    va_start(ap, fmt);
    vsnprintf(user_msg, sizeof(user_msg), fmt, ap);
    va_end(ap);

    if (errnum != 0) {
        snprintf(err_msg, sizeof(err_msg), ": %s (errno=%d)", strerror_r(errnum, errbuf, sizeof(errbuf)), errnum);
    } else {
        err_msg[0] = '\0';
    }

    n = snprintf(buf, sizeof(buf), "%s%s%s\n", prefix, user_msg, err_msg);
    if (n > 0) {
        if ((size_t)n >= sizeof(buf)) {
            n = (int)sizeof(buf) - 1;
            buf[n - 1] = '\n';
        }
        write_all(STDERR_FILENO, buf, (size_t)n);
    }

    errno = saved_errno;
}