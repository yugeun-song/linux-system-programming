#define _GNU_SOURCE

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

#include "utils/log.h"

#define FIELD_MAX 65535

enum len_mod { LEN_NONE, LEN_HH, LEN_H, LEN_L, LEN_LL, LEN_Z, LEN_J, LEN_T, LEN_BIG_L };

struct outbuf {
    char *buf;
    size_t cap;
    size_t len;
    size_t indent;
    int at_line_start;
};

struct spec {
    int left;
    int plus;
    int space;
    int alt;
    int zero;
    int width;
    int prec;
    enum len_mod len;
    char conv;
};

static ssize_t write_all(int fd, const void *buf, size_t count)
{
    const char *ptr = buf;
    size_t left = count;
    struct timespec zero = { 0, 0 };
    sigset_t guard;
    sigset_t old_set;
    sigset_t pending;
    siginfo_t si;
    int raised = 0;

    sigemptyset(&guard);
    sigaddset(&guard, SIGPIPE);
    sigaddset(&guard, SIGXFSZ);
    sigemptyset(&pending);
    sigpending(&pending);
    pthread_sigmask(SIG_BLOCK, &guard, &old_set);

    while (left > 0) {
        ssize_t n = write(fd, ptr, left);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) {
                continue;
            }
            if (n < 0 && errno == EPIPE) {
                raised = SIGPIPE;
            } else if (n < 0 && errno == EFBIG) {
                raised = SIGXFSZ;
            }
            break;
        }
        left -= (size_t)n;
        ptr += n;
    }

    if (raised != 0) {
        sigset_t one;
        int was_pending = sigismember(&pending, raised);
        int ours = !was_pending;

        sigemptyset(&one);
        sigaddset(&one, raised);
        if (sigtimedwait(&one, &si, &zero) == raised) {
            if (!ours) {
                sigemptyset(&pending);
                sigpending(&pending);
                ours = si.si_code == SI_USER && si.si_pid == getpid() && sigismember(&pending, raised);
            }
            if (!ours) {
                raise(raised);
            }
        } else if (ours) {
            sigemptyset(&pending);
            sigpending(&pending);
            if (sigismember(&pending, raised)) {
                sigaddset(&old_set, raised);
            }
        }
    }
    pthread_sigmask(SIG_SETMASK, &old_set, NULL);

    return (left == 0) ? (ssize_t)count : -1;
}

static void put_raw(struct outbuf *ob, char c)
{
    if (ob->len < ob->cap) {
        ob->buf[ob->len] = c;
    }
    ++ob->len;
}

static void put_char(struct outbuf *ob, char c)
{
    size_t i;

    if (c == '\n') {
        ob->at_line_start = 1;
    } else if (ob->at_line_start) {
        ob->at_line_start = 0;
        for (i = 0; i < ob->indent; ++i) {
            if (ob->len >= ob->cap) {
                ob->len += ob->indent - i;
                break;
            }
            put_raw(ob, ' ');
        }
    }
    put_raw(ob, c);
}

static void put_mem(struct outbuf *ob, const char *s, size_t n)
{
    size_t i;

    for (i = 0; i < n; ++i) {
        put_char(ob, s[i]);
    }
}

static void put_fill(struct outbuf *ob, char c, size_t n)
{
    while (n > 0) {
        put_char(ob, c);
        --n;
    }
}

static void put_field(struct outbuf *ob, const struct spec *sp, int zpad, const char *pfx,
                      size_t npfx, size_t nzeros, const char *body, size_t nbody)
{
    size_t total = npfx + nzeros + nbody;
    size_t pad = ((size_t)sp->width > total) ? (size_t)sp->width - total : 0;

    if (sp->left) {
        put_mem(ob, pfx, npfx);
        put_fill(ob, '0', nzeros);
        put_mem(ob, body, nbody);
        put_fill(ob, ' ', pad);
    } else if (zpad) {
        put_mem(ob, pfx, npfx);
        put_fill(ob, '0', nzeros + pad);
        put_mem(ob, body, nbody);
    } else {
        put_fill(ob, ' ', pad);
        put_mem(ob, pfx, npfx);
        put_fill(ob, '0', nzeros);
        put_mem(ob, body, nbody);
    }
}

static size_t fmt_udigits(char *out, unsigned long long v, unsigned base, int upper)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[24];
    size_t n = 0;
    size_t i;

    while (v != 0) {
        tmp[n] = digits[v % base];
        ++n;
        v /= base;
    }
    for (i = 0; i < n; ++i) {
        out[i] = tmp[n - 1 - i];
    }
    return n;
}

static void conv_int(struct outbuf *ob, const struct spec *sp, int neg, unsigned long long mag)
{
    char body[24];
    char pfx[2];
    size_t npfx = 0;
    size_t nzeros = 0;
    size_t nbody;
    unsigned base = 10;

    if (sp->conv == 'o') {
        base = 8;
    } else if (sp->conv == 'x' || sp->conv == 'X') {
        base = 16;
    }

    nbody = fmt_udigits(body, mag, base, sp->conv == 'X');
    if (sp->prec < 0) {
        if (nbody == 0) {
            body[0] = '0';
            nbody = 1;
        }
    } else if ((size_t)sp->prec > nbody) {
        nzeros = (size_t)sp->prec - nbody;
    }

    if (sp->conv == 'd' || sp->conv == 'i') {
        if (neg) {
            pfx[0] = '-';
            npfx = 1;
        } else if (sp->plus) {
            pfx[0] = '+';
            npfx = 1;
        } else if (sp->space) {
            pfx[0] = ' ';
            npfx = 1;
        }
    } else if (sp->alt && sp->conv == 'o' && nzeros == 0 && (nbody == 0 || body[0] != '0')) {
        nzeros = 1;
    } else if (sp->alt && base == 16 && mag != 0) {
        pfx[0] = '0';
        pfx[1] = sp->conv;
        npfx = 2;
    }

    put_field(ob, sp, sp->zero && !sp->left && sp->prec < 0, pfx, npfx, nzeros, body, nbody);
}

static void conv_str(struct outbuf *ob, const struct spec *sp, const char *s)
{
    size_t n;

    if (s == NULL) {
        s = (sp->prec < 0 || sp->prec >= 6) ? "(null)" : "";
    }
    n = (sp->prec < 0) ? strlen(s) : strnlen(s, (size_t)sp->prec);
    put_field(ob, sp, 0, "", 0, 0, s, n);
}

static void conv_ptr(struct outbuf *ob, const struct spec *sp, const void *ptr)
{
    struct spec hex = *sp;

    if (ptr == NULL) {
        put_field(ob, sp, 0, "", 0, 0, "(nil)", 5);
        return;
    }
    hex.conv = 'x';
    hex.alt = 1;
    hex.prec = -1;
    conv_int(ob, &hex, 0, (unsigned long long)(uintptr_t)ptr);
}

static const char *errdesc(int errnum)
{
    const char *desc = strerrordesc_np(errnum);

    return (desc != NULL) ? desc : "Unknown error";
}

static size_t log_vformat(char *buf, size_t cap, size_t indent, const char *fmt, va_list ap)
{
    struct outbuf ob = { buf, cap, 0, indent, 0 };
    const char *p = (fmt != NULL) ? fmt : "(null)";

    while (*p != '\0') {
        const char *start = p;
        struct spec sp = { 0 };
        int raw = 0;

        if (*p != '%') {
            put_char(&ob, *p);
            ++p;
            continue;
        }
        ++p;

        for (;;) {
            if (*p == '-') {
                sp.left = 1;
            } else if (*p == '+') {
                sp.plus = 1;
            } else if (*p == ' ') {
                sp.space = 1;
            } else if (*p == '#') {
                sp.alt = 1;
            } else if (*p == '0') {
                sp.zero = 1;
            } else if (*p != '\'' && *p != 'I') {
                break;
            }
            ++p;
        }

        if (*p == '*') {
            int w = va_arg(ap, int);

            if (w < 0) {
                sp.left = 1;
                w = (w == INT_MIN) ? INT_MAX : -w;
            }
            sp.width = (w > FIELD_MAX) ? FIELD_MAX : w;
            ++p;
        } else {
            while (*p >= '0' && *p <= '9') {
                sp.width = sp.width * 10 + (*p - '0');
                if (sp.width > FIELD_MAX) {
                    sp.width = FIELD_MAX;
                }
                ++p;
            }
        }

        sp.prec = -1;
        if (*p == '.') {
            ++p;
            sp.prec = 0;
            if (*p == '*') {
                int pr = va_arg(ap, int);

                sp.prec = (pr < 0) ? -1 : ((pr > FIELD_MAX) ? FIELD_MAX : pr);
                ++p;
            } else {
                while (*p >= '0' && *p <= '9') {
                    sp.prec = sp.prec * 10 + (*p - '0');
                    if (sp.prec > FIELD_MAX) {
                        sp.prec = FIELD_MAX;
                    }
                    ++p;
                }
            }
        }

        if (*p == 'h') {
            ++p;
            sp.len = LEN_H;
            if (*p == 'h') {
                ++p;
                sp.len = LEN_HH;
            }
        } else if (*p == 'l') {
            ++p;
            sp.len = LEN_L;
            if (*p == 'l') {
                ++p;
                sp.len = LEN_LL;
            }
        } else if (*p == 'z') {
            ++p;
            sp.len = LEN_Z;
        } else if (*p == 'j') {
            ++p;
            sp.len = LEN_J;
        } else if (*p == 't') {
            ++p;
            sp.len = LEN_T;
        } else if (*p == 'L') {
            ++p;
            sp.len = LEN_BIG_L;
        } else if (*p == 'q') {
            ++p;
            sp.len = LEN_LL;
        }

        sp.conv = *p;
        if (sp.conv == '\0') {
            put_mem(&ob, start, (size_t)(p - start));
            break;
        }
        ++p;

        switch (sp.conv) {
        case 'd':
        case 'i': {
            long long v;

            switch (sp.len) {
            case LEN_HH:    v = (signed char)va_arg(ap, int); break;
            case LEN_H:     v = (short)va_arg(ap, int); break;
            case LEN_L:     v = va_arg(ap, long); break;
            case LEN_LL:
            case LEN_BIG_L: v = va_arg(ap, long long); break;
            case LEN_Z:     v = va_arg(ap, ssize_t); break;
            case LEN_J:     v = va_arg(ap, intmax_t); break;
            case LEN_T:     v = va_arg(ap, ptrdiff_t); break;
            default:        v = va_arg(ap, int); break;
            }
            conv_int(&ob, &sp, v < 0, (v < 0) ? (unsigned long long)(-(v + 1)) + 1 : (unsigned long long)v);
            break;
        }
        case 'u':
        case 'o':
        case 'x':
        case 'X': {
            unsigned long long v;

            switch (sp.len) {
            case LEN_HH:    v = (unsigned char)va_arg(ap, unsigned int); break;
            case LEN_H:     v = (unsigned short)va_arg(ap, unsigned int); break;
            case LEN_L:     v = va_arg(ap, unsigned long); break;
            case LEN_LL:
            case LEN_BIG_L: v = va_arg(ap, unsigned long long); break;
            case LEN_Z:
            case LEN_T:     v = va_arg(ap, size_t); break;
            case LEN_J:     v = va_arg(ap, uintmax_t); break;
            default:        v = va_arg(ap, unsigned int); break;
            }
            conv_int(&ob, &sp, 0, v);
            break;
        }
        case 'c':
            if (sp.len == LEN_L) {
                (void)va_arg(ap, wint_t);
                raw = 1;
            } else {
                char ch = (char)va_arg(ap, int);

                put_field(&ob, &sp, 0, "", 0, 0, &ch, 1);
            }
            break;
        case 's':
            if (sp.len == LEN_L) {
                (void)va_arg(ap, const wchar_t *);
                raw = 1;
            } else {
                conv_str(&ob, &sp, va_arg(ap, const char *));
            }
            break;
        case 'p': conv_ptr(&ob, &sp, va_arg(ap, const void *)); break;
        case 'm': conv_str(&ob, &sp, errdesc(errno)); break;
        case 'n':
            (void)va_arg(ap, void *);
            raw = 1;
            break;
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A': {
            if (sp.len == LEN_BIG_L) {
                (void)va_arg(ap, long double);
            } else {
                (void)va_arg(ap, double);
            }
            raw = 1;
            break;
        }
        case '%': put_char(&ob, '%'); break;
        default:
            put_mem(&ob, start, (size_t)(p - start));
            put_mem(&ob, p, strlen(p));
            p += strlen(p);
            continue;
        }
        if (raw) {
            put_mem(&ob, start, (size_t)(p - start));
        }
    }

    if (cap > 0) {
        buf[(ob.len < cap) ? ob.len : cap - 1] = '\0';
    }
    return ob.len;
}

__attribute__((format(printf, 3, 4))) static size_t log_format(char *buf, size_t cap, const char *fmt, ...)
{
    va_list ap;
    size_t n;

    va_start(ap, fmt);
    n = log_vformat(buf, cap, 0, fmt, ap);
    va_end(ap);

    return n;
}

void log_emit(const char *level, const char *file, int line, const char *func, int errnum,
              enum log_padding padding, const char *fmt, ...)
{
    int saved_errno = errno;
    char buf[1024];
    char err_msg[160];
    struct timespec ts = {
        0,
    };
    time_t sod;
    va_list ap;
    size_t cap;
    size_t elen;
    size_t indent;
    size_t n;

    clock_gettime(CLOCK_REALTIME, &ts);
    sod = ts.tv_sec % 86400;
    if (sod < 0) {
        sod += 86400;
    }

    if (errnum != 0) {
        log_format(err_msg, sizeof(err_msg), ": %s (errno=%d)", errdesc(errnum), errnum);
    } else {
        err_msg[0] = '\0';
    }
    elen = strlen(err_msg);
    cap = sizeof(buf) - elen - 1;

    n = log_format(buf,
                   cap,
                   "%02d:%02d:%02d.%03ld [%-4s] [%d/%d] %s:%d %s(): ",
                   (int)(sod / 3600),
                   (int)(sod / 60 % 60),
                   (int)(sod % 60),
                   ts.tv_nsec / 1000000,
                   level,
                   getpid(),
                   gettid(),
                   file,
                   line,
                   func);
    if (n > cap - 1) {
        n = cap - 1;
    }

    indent = (padding == LOG_PADDING_ON) ? n : 0;

    va_start(ap, fmt);
    n += log_vformat(buf + n, cap - n, indent, fmt, ap);
    va_end(ap);
    if (n > cap - 1) {
        n = cap - 1;
    }

    memcpy(buf + n, err_msg, elen);
    n += elen;
    buf[n] = '\n';
    ++n;
    write_all(STDERR_FILENO, buf, n);

    errno = saved_errno;
}
