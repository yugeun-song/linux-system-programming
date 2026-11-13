#define _GNU_SOURCE

#include "utils/log.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#define FIELD_MAX 65535

enum len_mod {
    LEN_NONE,
    LEN_HH,
    LEN_H,
    LEN_L,
    LEN_LL,
    LEN_Z,
    LEN_J,
    LEN_T,
    LEN_BIG_L,
};

struct outbuf {
    char *buf;
    size_t cap;
    size_t len;
    size_t indent;
    int is_at_line_start;
};

struct spec {
    int is_left_justified;
    int shows_sign;
    int shows_space;
    int is_alternate_form;
    int pads_with_zero;
    int width;
    int prec;
    enum len_mod len;
    char conv;
};

static ssize_t write_all(int fd, const void *const buf, size_t count)
{
    const char *cursor = buf;
    size_t remaining = count;
    sigset_t guard_set;
    sigset_t old_set;
    sigset_t pending_set;
    int raised_signal = 0;

    sigemptyset(&guard_set);
    sigaddset(&guard_set, SIGPIPE);
    sigaddset(&guard_set, SIGXFSZ);
    sigemptyset(&pending_set);
    sigpending(&pending_set);
    pthread_sigmask(SIG_BLOCK, &guard_set, &old_set);

    while (remaining > 0) {
        ssize_t written = write(fd, cursor, remaining);
        if (written <= 0) {
            if (written < 0 && errno == EINTR) {
                continue;
            }
            if (written < 0 && errno == EPIPE) {
                raised_signal = SIGPIPE;
            } else if (written < 0 && errno == EFBIG) {
                raised_signal = SIGXFSZ;
            }
            break;
        }
        remaining -= (size_t)written;
        cursor += written;
    }

    if (raised_signal != 0) {
        struct timespec zero_timeout = { 0 };
        siginfo_t info;
        int was_pending = sigismember(&pending_set, raised_signal);
        int is_own_signal = (was_pending == 0);

        sigdelset(&guard_set, (raised_signal == SIGPIPE) ? SIGXFSZ : SIGPIPE);
        if (sigtimedwait(&guard_set, &info, &zero_timeout) == raised_signal) {
            if (is_own_signal == 0) {
                sigemptyset(&pending_set);
                sigpending(&pending_set);
                is_own_signal = info.si_code == SI_USER && info.si_pid == getpid() &&
                                sigismember(&pending_set, raised_signal) != 0;
            }
            if (is_own_signal == 0) {
                raise(raised_signal);
            }
        } else if (is_own_signal != 0) {
            sigemptyset(&pending_set);
            sigpending(&pending_set);
            if (sigismember(&pending_set, raised_signal) != 0) {
                sigaddset(&old_set, raised_signal);
            }
        }
    }
    pthread_sigmask(SIG_SETMASK, &old_set, NULL);

    return (remaining == 0) ? (ssize_t)count : -1;
}

static void put_raw(struct outbuf *const mut_out, char c)
{
    if (mut_out->len < mut_out->cap) {
        mut_out->buf[mut_out->len] = c;
    }
    ++mut_out->len;
}

static void put_char(struct outbuf *const mut_out, char c)
{
    if (c == '\n') {
        mut_out->is_at_line_start = 1;
    } else if (mut_out->is_at_line_start != 0) {
        mut_out->is_at_line_start = 0;
        for (size_t i = 0; i < mut_out->indent; ++i) {
            if (mut_out->len >= mut_out->cap) {
                mut_out->len += mut_out->indent - i;
                break;
            }
            put_raw(mut_out, ' ');
        }
    }
    put_raw(mut_out, c);
}

static void put_mem(struct outbuf *const mut_out, const char *const text, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        put_char(mut_out, text[i]);
    }
}

static void put_fill(struct outbuf *const mut_out, char c, size_t n)
{
    while (n > 0) {
        put_char(mut_out, c);
        --n;
    }
}

static void put_field(struct outbuf *const mut_out, const struct spec *const spec,
                      int pads_with_zero, const char *const prefix, size_t prefix_len,
                      size_t zero_count, const char *const body, size_t body_len)
{
    size_t total = prefix_len + zero_count + body_len;
    size_t pad = ((size_t)spec->width > total) ? (size_t)spec->width - total : 0;

    if (spec->is_left_justified != 0) {
        put_mem(mut_out, prefix, prefix_len);
        put_fill(mut_out, '0', zero_count);
        put_mem(mut_out, body, body_len);
        put_fill(mut_out, ' ', pad);
    } else if (pads_with_zero != 0) {
        put_mem(mut_out, prefix, prefix_len);
        put_fill(mut_out, '0', zero_count + pad);
        put_mem(mut_out, body, body_len);
    } else {
        put_fill(mut_out, ' ', pad);
        put_mem(mut_out, prefix, prefix_len);
        put_fill(mut_out, '0', zero_count);
        put_mem(mut_out, body, body_len);
    }
}

static size_t fmt_udigits(char *const mut_buf, unsigned long long value, unsigned base,
                          int is_upper)
{
    const char *digits = (is_upper != 0) ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[24];
    size_t n = 0;

    while (value != 0) {
        tmp[n] = digits[value % base];
        ++n;
        value /= base;
    }
    for (size_t i = 0; i < n; ++i) {
        mut_buf[i] = tmp[n - 1 - i];
    }
    return n;
}

static unsigned long long compute_magnitude(long long value)
{
    if (value < 0) {
        return (unsigned long long)(-(value + 1)) + 1;
    }
    return (unsigned long long)value;
}

static void conv_int(struct outbuf *const mut_out, const struct spec *const spec, int is_negative,
                     unsigned long long magnitude)
{
    char body[24];
    char prefix[3];
    size_t prefix_len = 0;
    size_t zero_count = 0;
    unsigned base = 10;

    if (spec->conv == 'o') {
        base = 8;
    } else if (spec->conv == 'x' || spec->conv == 'X' || spec->conv == 'p') {
        base = 16;
    }

    size_t body_len = fmt_udigits(body, magnitude, base, spec->conv == 'X');
    if (spec->prec < 0) {
        if (body_len == 0) {
            body[0] = '0';
            body_len = 1;
        }
    } else if ((size_t)spec->prec > body_len) {
        zero_count = (size_t)spec->prec - body_len;
    }

    if (spec->conv == 'd' || spec->conv == 'i' || spec->conv == 'p') {
        if (is_negative != 0) {
            prefix[prefix_len] = '-';
            ++prefix_len;
        } else if (spec->shows_sign != 0) {
            prefix[prefix_len] = '+';
            ++prefix_len;
        } else if (spec->shows_space != 0) {
            prefix[prefix_len] = ' ';
            ++prefix_len;
        }
    }
    if (spec->is_alternate_form != 0 && spec->conv == 'o' && zero_count == 0 &&
        (body_len == 0 || body[0] != '0')) {
        zero_count = 1;
    } else if (spec->is_alternate_form != 0 && base == 16 && magnitude != 0) {
        prefix[prefix_len] = '0';
        ++prefix_len;
        prefix[prefix_len] = (spec->conv == 'X') ? 'X' : 'x';
        ++prefix_len;
    }

    int pads_with_zero = spec->pads_with_zero != 0 && spec->is_left_justified == 0 &&
                         spec->prec < 0;

    put_field(mut_out, spec, pads_with_zero, prefix, prefix_len, zero_count, body, body_len);
}

static void conv_str(struct outbuf *const mut_out, const struct spec *const spec,
                     const char *const text_nullable)
{
    const char *text = text_nullable;

    if (text == NULL) {
        text = (spec->prec < 0 || spec->prec >= 6) ? "(null)" : "";
    }

    size_t len = (spec->prec < 0) ? strlen(text) : strnlen(text, (size_t)spec->prec);

    put_field(mut_out, spec, 0, "", 0, 0, text, len);
}

static void conv_ptr(struct outbuf *const mut_out, const struct spec *const spec,
                     const void *const ptr_nullable)
{
    struct spec hex = *spec;

    if (ptr_nullable == NULL) {
        put_field(mut_out, spec, 0, "", 0, 0, "(nil)", 5);
        return;
    }
    hex.is_alternate_form = 1;
    conv_int(mut_out, &hex, 0, (unsigned long long)(uintptr_t)ptr_nullable);
}

static const char *describe_error(int errnum)
{
    const char *description = strerrordesc_np(errnum);

    return (description != NULL) ? description : "Unknown error";
}

static void conv_errno(struct outbuf *const mut_out, const struct spec *const spec, int errnum)
{
    char text[40];
    const char *message = NULL;
    unsigned long long magnitude = compute_magnitude(errnum);

    if (spec->is_alternate_form != 0) {
        message = strerrorname_np(errnum);
    } else {
        message = strerrordesc_np(errnum);
    }

    if (message == NULL && spec->is_alternate_form != 0) {
        struct spec decimal = *spec;

        decimal.conv = 'd';
        conv_int(mut_out, &decimal, errnum < 0, magnitude);
        return;
    }
    if (message == NULL) {
        size_t len = sizeof("Unknown error ") - 1;

        memcpy(text, "Unknown error ", len);
        if (errnum < 0) {
            text[len] = '-';
            ++len;
        }
        len += fmt_udigits(text + len, magnitude, 10, 0);
        text[len] = '\0';
        message = text;
    }
    conv_str(mut_out, spec, message);
}

static size_t log_vformat(char *const mut_buf, size_t cap, size_t indent,
                          const char *const fmt_nullable, va_list *const mut_ap)
{
    struct outbuf out = {
        .buf = mut_buf,
        .cap = cap,
        .len = 0,
        .indent = indent,
        .is_at_line_start = 0
    };
    const char *cursor = (fmt_nullable != NULL) ? fmt_nullable : "(null)";

    while (*cursor != '\0') {
        const char *start = cursor;
        struct spec spec = { 0 };
        int is_verbatim = 0;

        if (*cursor != '%') {
            put_char(&out, *cursor);
            ++cursor;
            continue;
        }
        ++cursor;

        for (;;) {
            if (*cursor == '-') {
                spec.is_left_justified = 1;
            } else if (*cursor == '+') {
                spec.shows_sign = 1;
            } else if (*cursor == ' ') {
                spec.shows_space = 1;
            } else if (*cursor == '#') {
                spec.is_alternate_form = 1;
            } else if (*cursor == '0') {
                spec.pads_with_zero = 1;
            } else if (*cursor != '\'' && *cursor != 'I') {
                break;
            }
            ++cursor;
        }

        if (*cursor == '*') {
            int width = va_arg(*mut_ap, int);

            if (width < 0) {
                spec.is_left_justified = 1;
                width = (width == INT_MIN) ? INT_MAX : -width;
            }
            spec.width = (width > FIELD_MAX) ? FIELD_MAX : width;
            ++cursor;
        } else {
            while (*cursor >= '0' && *cursor <= '9') {
                spec.width = spec.width * 10 + (*cursor - '0');
                if (spec.width > FIELD_MAX) {
                    spec.width = FIELD_MAX;
                }
                ++cursor;
            }
        }

        spec.prec = -1;
        if (*cursor == '.') {
            ++cursor;
            spec.prec = 0;
            if (*cursor == '*') {
                int precision = va_arg(*mut_ap, int);

                if (precision < 0) {
                    spec.prec = -1;
                } else if (precision > FIELD_MAX) {
                    spec.prec = FIELD_MAX;
                } else {
                    spec.prec = precision;
                }
                ++cursor;
            } else {
                while (*cursor >= '0' && *cursor <= '9') {
                    spec.prec = spec.prec * 10 + (*cursor - '0');
                    if (spec.prec > FIELD_MAX) {
                        spec.prec = FIELD_MAX;
                    }
                    ++cursor;
                }
            }
        }

        if (*cursor == 'h') {
            ++cursor;
            spec.len = LEN_H;
            if (*cursor == 'h') {
                ++cursor;
                spec.len = LEN_HH;
            }
        } else if (*cursor == 'l') {
            ++cursor;
            spec.len = LEN_L;
            if (*cursor == 'l') {
                ++cursor;
                spec.len = LEN_LL;
            }
        } else if (*cursor == 'z') {
            ++cursor;
            spec.len = LEN_Z;
        } else if (*cursor == 'j') {
            ++cursor;
            spec.len = LEN_J;
        } else if (*cursor == 't') {
            ++cursor;
            spec.len = LEN_T;
        } else if (*cursor == 'L') {
            ++cursor;
            spec.len = LEN_BIG_L;
        } else if (*cursor == 'q') {
            ++cursor;
            spec.len = LEN_LL;
        }

        spec.conv = *cursor;
        if (spec.conv == '\0') {
            put_mem(&out, start, (size_t)(cursor - start));
            break;
        }
        ++cursor;

        switch (spec.conv) {
        case 'd':
        case 'i': {
            long long value = 0;

            switch (spec.len) {
            case LEN_HH:
                value = (signed char)va_arg(*mut_ap, int);
                break;
            case LEN_H:
                value = (short)va_arg(*mut_ap, int);
                break;
            case LEN_L:
                value = va_arg(*mut_ap, long);
                break;
            case LEN_LL:
            case LEN_BIG_L:
                value = va_arg(*mut_ap, long long);
                break;
            case LEN_Z:
                value = va_arg(*mut_ap, ssize_t);
                break;
            case LEN_J:
                value = va_arg(*mut_ap, intmax_t);
                break;
            case LEN_T:
                value = va_arg(*mut_ap, ptrdiff_t);
                break;
            case LEN_NONE:
                value = va_arg(*mut_ap, int);
                break;
            }
            conv_int(&out, &spec, value < 0, compute_magnitude(value));
            break;
        }
        case 'u':
        case 'o':
        case 'x':
        case 'X': {
            unsigned long long value = 0;

            switch (spec.len) {
            case LEN_HH:
                value = (unsigned char)va_arg(*mut_ap, unsigned int);
                break;
            case LEN_H:
                value = (unsigned short)va_arg(*mut_ap, unsigned int);
                break;
            case LEN_L:
                value = va_arg(*mut_ap, unsigned long);
                break;
            case LEN_LL:
            case LEN_BIG_L:
                value = va_arg(*mut_ap, unsigned long long);
                break;
            case LEN_Z:
            case LEN_T:
                value = va_arg(*mut_ap, size_t);
                break;
            case LEN_J:
                value = va_arg(*mut_ap, uintmax_t);
                break;
            case LEN_NONE:
                value = va_arg(*mut_ap, unsigned int);
                break;
            }
            conv_int(&out, &spec, 0, value);
            break;
        }
        case 'c':
            if (spec.len == LEN_L) {
                va_arg(*mut_ap, wint_t);
                is_verbatim = 1;
            } else {
                char ch = (char)va_arg(*mut_ap, int);

                put_field(&out, &spec, 0, "", 0, 0, &ch, 1);
            }
            break;
        case 's':
            if (spec.len == LEN_L) {
                va_arg(*mut_ap, const wchar_t *);
                is_verbatim = 1;
            } else {
                conv_str(&out, &spec, va_arg(*mut_ap, const char *));
            }
            break;
        case 'p':
            conv_ptr(&out, &spec, va_arg(*mut_ap, const void *));
            break;
        case 'm':
            conv_errno(&out, &spec, errno);
            break;
        case 'n':
            va_arg(*mut_ap, void *);
            is_verbatim = 1;
            break;
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A': {
            if (spec.len == LEN_BIG_L) {
                va_arg(*mut_ap, long double);
            } else {
                va_arg(*mut_ap, double);
            }
            is_verbatim = 1;
            break;
        }
        case '%':
            put_char(&out, '%');
            break;
        default:
            put_mem(&out, start, (size_t)(cursor - start));
            put_mem(&out, cursor, strlen(cursor));
            cursor += strlen(cursor);
            continue;
        }
        if (is_verbatim != 0) {
            put_mem(&out, start, (size_t)(cursor - start));
        }
    }

    if (cap > 0) {
        mut_buf[(out.len < cap) ? out.len : cap - 1] = '\0';
    }
    return out.len;
}

static __attribute__((format(printf, 3, 4))) size_t log_format(char *const mut_buf, size_t cap,
                                                               const char *const fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    size_t len = log_vformat(mut_buf, cap, 0, fmt, &ap);
    va_end(ap);

    return len;
}

void log_emit(const char *const level, const char *const file, int line, const char *const func,
              int errnum, enum print_padding padding, const char *const fmt_nullable, ...)
{
    int saved_errno = errno;
    char buf[1024];
    const char *description = "";
    struct timespec now = { 0 };

    clock_gettime(CLOCK_REALTIME, &now);
    time_t seconds_of_day = now.tv_sec % 86400;
    if (seconds_of_day < 0) {
        seconds_of_day += 86400;
    }

    size_t tail_len = 0;
    if (errnum != 0) {
        unsigned long long magnitude = compute_magnitude(errnum);

        description = describe_error(errnum);
        tail_len = strlen(description) + sizeof(": ") + sizeof(" (errno=") + sizeof(")") - 3;
        if (errnum < 0) {
            ++tail_len;
        }
        do {
            ++tail_len;
            magnitude /= 10;
        } while (magnitude != 0);
    }
    size_t cap = sizeof(buf) - tail_len - 1;

    size_t len = log_format(buf,
                            cap,
                            "%02d:%02d:%02d.%03ld [%-4s] [%d/%d] %s:%d %s(): ",
                            (int)(seconds_of_day / 3600),
                            (int)((seconds_of_day / 60) % 60),
                            (int)(seconds_of_day % 60),
                            now.tv_nsec / 1000000,
                            level,
                            getpid(),
                            gettid(),
                            file,
                            line,
                            func);
    if (len > cap - 1) {
        len = cap - 1;
    }

    size_t indent = (padding == PRINT_PADDING_ON) ? len : 0;
    va_list ap;

    va_start(ap, fmt_nullable);
    len += log_vformat(buf + len, cap - len, indent, fmt_nullable, &ap);
    va_end(ap);
    if (len > cap - 1) {
        len = cap - 1;
    }

    if (errnum != 0) {
        len += log_format(buf + len, sizeof(buf) - len, ": %s (errno=%d)", description, errnum);
        if (len > sizeof(buf) - 2) {
            len = sizeof(buf) - 2;
        }
    }
    buf[len] = '\n';
    ++len;
    write_all(STDERR_FILENO, buf, len);

    errno = saved_errno;
}
