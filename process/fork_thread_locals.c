#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>

#include "helper/log.h"

#define NUM_THREADS 3

static __thread int tls_value;

struct thread_slot {
    pthread_t tid;
    int *local_ptr;
    int *tls_ptr;
};

static struct thread_slot g_slots[NUM_THREADS];
static pthread_barrier_t g_ready;

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

static void *worker_routine(void *arg)
{
    long idx = (long)arg;
    int local_marker = (int)(1000 + idx * 100);

    tls_value = (int)(2000 + idx * 100);

    g_slots[idx].local_ptr = &local_marker;
    g_slots[idx].tls_ptr = &tls_value;

    printf("worker_routine(): thread %ld set local=%d tls=%d\n", idx, local_marker, tls_value);

    pthread_barrier_wait(&g_ready);

    pause();
    return NULL;
}

static void child_report(int caller_local)
{
    char buf[256 + NUM_THREADS * 128];
    size_t off = 0;
    int i;

    static const char h1[] = "child_report(): only the calling thread survived the fork\n";
    static const char h2[] = "child_report(): caller local = ";
    static const char h3[] = "child_report(): direct __thread tls = ";
    static const char h3b[] = " (this survivor thread's own)\n";
    static const char w1[] = "child_report(): worker ";
    static const char w2[] = " frozen local=";
    static const char w3[] = " tls=";
    static const char w4[] = " (thread gone, memory copied)\n";

    off = put_lit(buf, off, h1, sizeof(h1) - 1);

    off = put_lit(buf, off, h2, sizeof(h2) - 1);
    off = put_dec(buf, off, (unsigned long long)(unsigned int)caller_local);
    buf[off] = '\n';
    ++off;

    off = put_lit(buf, off, h3, sizeof(h3) - 1);
    off = put_dec(buf, off, (unsigned long long)(unsigned int)tls_value);
    off = put_lit(buf, off, h3b, sizeof(h3b) - 1);

    for (i = 0; i < NUM_THREADS; ++i) {
        off = put_lit(buf, off, w1, sizeof(w1) - 1);
        off = put_dec(buf, off, (unsigned long long)(unsigned int)i);
        off = put_lit(buf, off, w2, sizeof(w2) - 1);
        off = put_dec(buf, off, (unsigned long long)(unsigned int)(*g_slots[i].local_ptr));
        off = put_lit(buf, off, w3, sizeof(w3) - 1);
        off = put_dec(buf, off, (unsigned long long)(unsigned int)(*g_slots[i].tls_ptr));
        off = put_lit(buf, off, w4, sizeof(w4) - 1);
    }

    write(STDOUT_FILENO, buf, off);
}

int main(void)
{
    long i;
    int rc;
    int caller_local = 42;

    setvbuf(stdout, NULL, _IOLBF, 0);

    tls_value = 9999;

    rc = pthread_barrier_init(&g_ready, NULL, NUM_THREADS + 1);
    if (rc != 0) {
        LOG_PERROR(rc, "pthread_barrier_init failed");
        return 1;
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        rc = pthread_create(&g_slots[i].tid, NULL, worker_routine, (void *)i);
        if (rc != 0) {
            LOG_PERROR(rc, "pthread_create failed");
            return 1;
        }
    }

    pthread_barrier_wait(&g_ready);

    printf("main(): spawned %d worker threads, forking from the main thread\n", NUM_THREADS);

    pid_t pid = fork();
    if (pid < 0) {
        LOG_PERROR(errno, "fork failed");
        return 1;
    }

    if (pid == 0) {
        child_report(caller_local);
        _exit(0);
    }

    while (waitpid(pid, NULL, 0) == -1) {
        if (errno != EINTR) {
            LOG_PERROR(errno, "waitpid failed");
            return 1;
        }
    }

    printf("main(): child exited\n");

    pthread_barrier_destroy(&g_ready);
    return 0;
}