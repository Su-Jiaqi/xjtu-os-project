// step3_exec_in_thread.c
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

void* worker_exec(void* _) {
    printf("[exec-thread] before exec: PID=%d TID=%d\n", getpid(), gettid_wrap());
    // 用当前目录下的 ./exec_helper 作为新程序
    char* const argv[] = { "./exec_helper", "argA", "argB", NULL };
    execv("./exec_helper", argv);

    // 正常情况下 execv 成功不会返回；若返回，表示失败
    perror("execv failed");
    return NULL;
}

int main(void) {
    printf("[main] PID=%d TID=%d\n", getpid(), gettid_wrap());
    pthread_t t;
    if (pthread_create(&t, NULL, worker_exec, NULL) != 0) { perror("pthread_create"); exit(1); }

    sleep(2);
    
    pthread_join(t, NULL);
    printf("[main] after join (this usually indicates execv failed)\n");
    return 0;
}
