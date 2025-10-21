// step3_system_in_thread.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

static pid_t gettid_wrap(void) {
    return (pid_t)syscall(SYS_gettid);
}

void* worker(void* _) {
    printf("[system-thread] PID=%d TID=%d : calling system(\"echo hi; sleep 1; echo done\")\n",
           getpid(), gettid_wrap());
    int rc = system("echo [from system child]; sleep 1; echo [system child done]");
    printf("[system-thread] PID=%d TID=%d : system() returned rc=%d\n",
           getpid(), gettid_wrap(), rc);
    return NULL;
}

int main(void) {
    printf("[main] PID=%d TID=%d\n", getpid(), gettid_wrap());
    pthread_t t;
    if (pthread_create(&t, NULL, worker, NULL) != 0) { perror("pthread_create"); exit(1); }
    pthread_join(t, NULL);
    printf("[main] PID=%d TID=%d : done\n", getpid(), gettid_wrap());
    return 0;
}
