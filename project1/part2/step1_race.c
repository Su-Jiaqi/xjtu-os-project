// step1_race.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

static long long g = 0;           // 共享变量，初值 0
static const int N = 100000;      // 循环次数

void* add_worker(void* _) {
    for (int i = 0; i < N; i++) {
        long long tmp = g;
        if ((i & 1023) == 0) sched_yield();
        tmp += 100;
        g = tmp;
    }
    return NULL;
}

void* sub_worker(void* _) {
    for (int i = 0; i < N; i++) {
        long long tmp = g;
        if ((i & 1023) == 0) sched_yield();
        tmp -= 100;
        g = tmp;
    }
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    printf("[STEP1] before: g=%lld\n", g);

    if (pthread_create(&t1, NULL, add_worker, NULL) != 0) { perror("pthread_create"); exit(1); }
    if (pthread_create(&t2, NULL, sub_worker, NULL) != 0) { perror("pthread_create"); exit(1); }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("[STEP1] after:  g=%lld (expect 0 if no race)\n", g);
    return 0;
}
