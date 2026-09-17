#define _GNU_SOURCE

#include <errno.h>

#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>

#include "utils/log.h"

#define NUM_THREADS 3

static __thread int g_tls_value;
static pthread_barrier_t g_ready_barrier;

struct thread_slot {
    pthread_t tid;
    int *local_ptr;
    int *tls_ptr;
};

static struct thread_slot g_slots[NUM_THREADS];

static void *worker_routine(void *arg)
{
    long worker_idx = (long)arg;
    int local_marker = (int)(1000 + worker_idx * 100);

    g_tls_value = (int)(2000 + worker_idx * 100);
    g_slots[worker_idx].local_ptr = &local_marker;
    g_slots[worker_idx].tls_ptr = &g_tls_value;

    LOG_INFO("thread %ld set local=%d tls=%d", worker_idx, local_marker, g_tls_value);

    pthread_barrier_wait(&g_ready_barrier);

    pause();
    return NULL;
}

static void child_report(int caller_local_value)
{
    int i;

    LOG_INFO("only the calling thread survived the fork");
    LOG_INFO("caller\'s local value = %d", caller_local_value);
    LOG_INFO("direct __thread tls = %d (this survivor thread's own)", g_tls_value);

    for (i = 0; i < NUM_THREADS; ++i) {
        LOG_INFO("worker %d frozen local=%d tls=%d (thread gone, memory copied)", i,
                 *g_slots[i].local_ptr, *g_slots[i].tls_ptr);
    }
}

int main(void)
{
    long i;
    int return_code;
    int caller_local_value = 42;

    g_tls_value = 9999;

    return_code = pthread_barrier_init(&g_ready_barrier, NULL, NUM_THREADS + 1);
    if (return_code != 0) {
        LOG_PERROR(return_code, "pthread_barrier_init failed");
        return 1;
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        return_code = pthread_create(&g_slots[i].tid, NULL, worker_routine, (void *)i);
        if (return_code != 0) {
            LOG_PERROR(return_code, "pthread_create failed");
            return 1;
        }
    }

    pthread_barrier_wait(&g_ready_barrier);

    LOG_INFO("spawned %d worker threads, forking from the main thread", NUM_THREADS);

    pid_t pid = fork();
    if (pid < 0) {
        LOG_PERROR(errno, "fork failed");
        return 1;
    } else if (pid == 0) {
        child_report(caller_local_value);
        _exit(0);
    }

    while (waitpid(pid, NULL, 0) == -1) {
        if (errno != EINTR) {
            LOG_PERROR(errno, "waitpid failed");
            return 1;
        }
    }

    LOG_INFO("child exited");

    pthread_barrier_destroy(&g_ready_barrier);
    return 0;
}