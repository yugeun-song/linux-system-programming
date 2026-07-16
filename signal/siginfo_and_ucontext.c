#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ucontext.h>

#if defined(__x86_64__)
#define UC_PC(uc) ((unsigned long long)(uc)->uc_mcontext.gregs[REG_RIP])
#define UC_SP(uc) ((unsigned long long)(uc)->uc_mcontext.gregs[REG_RSP])
#elif defined(__aarch64__)
#define UC_PC(uc) ((unsigned long long)(uc)->uc_mcontext.pc)
#define UC_SP(uc) ((unsigned long long)(uc)->uc_mcontext.sp)
#elif defined(__riscv) && (__riscv_xlen == 64)
#define UC_PC(uc) ((unsigned long long)(uc)->uc_mcontext.__gregs[REG_PC])
#define UC_SP(uc) ((unsigned long long)(uc)->uc_mcontext.__gregs[REG_SP])
#else
#error "unsupported architecture (need x86_64, aarch64, or rv64)"
#endif

static size_t put_lit(char *buf, size_t off, const char *s, size_t n)
{
    memcpy(buf + off, s, n);
    return off + n;
}

static size_t put_dec(char *buf, size_t off, unsigned long long v)
{
    char tmp[20];
    size_t n = 0;

    if (v == 0) {
        buf[off] = '0';
        return off + 1;
    }
    while (v != 0) {
        tmp[n] = (char)('0' + (v % 10));
        ++n;
        v /= 10;
    }
    while (n > 0) {
        --n;
        buf[off] = tmp[n];
        ++off;
    }
    return off;
}

static size_t put_hex64(char *buf, size_t off, unsigned long long v)
{
    int i;
    unsigned int nibble;

    for (i = 60; i >= 0; i -= 4) {
        nibble = (unsigned int)(v >> i) & 0xfu;
        if (nibble < 10u) {
            buf[off] = (char)('0' + (int)nibble);
        } else {
            buf[off] = (char)('a' + (int)nibble - 10);
        }
        ++off;
    }
    return off;
}

static const char *si_code_str(int code)
{
    switch (code) {
    case SI_USER:    return "SI_USER";
    case SI_KERNEL:  return "SI_KERNEL";
    case SI_QUEUE:   return "SI_QUEUE";
    case SI_TIMER:   return "SI_TIMER";
    case SI_MESGQ:   return "SI_MESGQ";
    case SI_ASYNCIO: return "SI_ASYNCIO";
    case SI_SIGIO:   return "SI_SIGIO";
    case SI_TKILL:   return "SI_TKILL";
    default:         return "?";
    }
}

static void signal_handler(int signum, siginfo_t *info, void *ucontext)
{
    const ucontext_t *uc = ucontext;
    const char *code_name = si_code_str(info->si_code);
    char buf[512];
    size_t off = 0;

    static const char l1[] = "signal_handler(): signum=";
    static const char l2[] = "                  si_code=";
    static const char l3[] = "                  si_pid=";
    static const char l4[] = "                  pc=0x";
    static const char l5[] = "                  sp=0x";

    off = put_lit(buf, off, l1, sizeof(l1) - 1);
    off = put_dec(buf, off, (unsigned long long)(unsigned int)signum);
    buf[off] = '\n';
    ++off;

    off = put_lit(buf, off, l2, sizeof(l2) - 1);
    off = put_lit(buf, off, code_name, strlen(code_name));
    buf[off] = '\n';
    ++off;

    off = put_lit(buf, off, l3, sizeof(l3) - 1);
    off = put_dec(buf, off, (unsigned long long)(unsigned int)info->si_pid);
    buf[off] = '\n';
    ++off;

    off = put_lit(buf, off, l4, sizeof(l4) - 1);
    off = put_hex64(buf, off, UC_PC(uc));
    buf[off] = '\n';
    ++off;

    off = put_lit(buf, off, l5, sizeof(l5) - 1);
    off = put_hex64(buf, off, UC_SP(uc));
    buf[off] = '\n';
    ++off;

    write(STDOUT_FILENO, buf, off);
}

int main(void)
{
    struct sigaction sa = { 0 };
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;

    if (sigemptyset(&sa.sa_mask) == -1) {
        perror("main(): failed to initialize signal set with sigemptyset");
        return EXIT_FAILURE;
    }

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("main(): failed to register SIGINT handler");
        return EXIT_FAILURE;
    }

    printf("main(): raising SIGINT to inspect signal context\n");

    if (raise(SIGINT) != 0) {
        perror("main(): failed to raise SIGINT");
        return EXIT_FAILURE;
    }

    printf("main(): finished\n");
    return EXIT_SUCCESS;
}