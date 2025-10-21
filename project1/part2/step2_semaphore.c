// step2_semaphore.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

static long long g = 0;
static const int N = 100000;
static sem_t s;   // 信号量（作为二值信号量/互斥量用）

// P 操作
static inline void P(sem_t* x) { sem_wait(x); }
// V 操作
static inline void V(sem_t* x) { sem_post(x); }

void* add_worker(void* _) {
    for (int i = 0; i < N; i++) {
        P(&s);
        long long tmp = g;
        tmp += 100;
        g = tmp;
        V(&s);
    }
    return NULL;
}

void* sub_worker(void* _) {
    for (int i = 0; i < N; i++) {
        P(&s);
        long long tmp = g;
        tmp -= 100;
        g = tmp;
        V(&s);
    }
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    // 初始化为 1：一次只有一个线程可进入临界区
    if (sem_init(&s, 0, 1) != 0) { perror("sem_init"); exit(1); }

    printf("[STEP2] before: g=%lld\n", g);

    if (pthread_create(&t1, NULL, add_worker, NULL) != 0) { perror("pthread_create"); exit(1); }
    if (pthread_create(&t2, NULL, sub_worker, NULL) != 0) { perror("pthread_create"); exit(1); }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("[STEP2] after:  g=%lld (should be 0)\n", g);
    sem_destroy(&s);
    return 0;
}
