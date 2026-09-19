#define _GNU_SOURCE

#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ucontext.h>

#include "utils/log.h"

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

static const char *si_code_to_str(int si_code)
{
    switch (si_code) {
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
    int saved_errno = errno;
    const ucontext_t *uc = ucontext;

    PRINT_INFO("signum=%d si_code=%s si_pid=%d pc=0x%016llx sp=0x%016llx",
               signum,
               si_code_to_str(info->si_code),
               info->si_pid,
               UC_PC(uc),
               UC_SP(uc));

    errno = saved_errno;
}

int main(void)
{
    struct sigaction sa = { 0 };
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;

    if (sigemptyset(&sa.sa_mask) == -1) {
        PRINT_PERROR(errno, "failed to initialize signal set with sigemptyset");
        return EXIT_FAILURE;
    }

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        PRINT_PERROR(errno, "failed to register SIGINT handler");
        return EXIT_FAILURE;
    }

    PRINT_INFO("raising SIGINT to inspect signal context");

    if (raise(SIGINT) != 0) {
        PRINT_PERROR(errno, "failed to raise SIGINT");
        return EXIT_FAILURE;
    }

    PRINT_INFO("finished");
    return EXIT_SUCCESS;
}