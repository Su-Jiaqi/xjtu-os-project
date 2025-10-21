/**
 * spinlock.c  (自旋锁实验)
 * XJTU 并发实验 Part 3
 */
 #include <stdio.h>
 #include <pthread.h>
 #include <stdlib.h>
 #include <unistd.h>
 
 
 typedef struct {
     int flag;  // 0 = unlocked, 1 = locked
 } spinlock_t;
 
 void spinlock_init(spinlock_t *lock) {
     lock->flag = 0;
 }
 
 // 获取锁（忙等待）
 void spinlock_lock(spinlock_t *lock) {
     while (__sync_lock_test_and_set(&lock->flag, 1)) {
     }
 }
 
 // 释放锁
 void spinlock_unlock(spinlock_t *lock) {
     __sync_lock_release(&lock->flag);
 }
 
 int shared_value = 0;
 const int N = 5000;  // 每个线程操作 5000 次
 
 void* thread_function(void *arg) {
     spinlock_t *lock = (spinlock_t *)arg;
     for (int i = 0; i < N; ++i) {
         spinlock_lock(lock);
         shared_value++;       // 临界区：累加共享变量
         spinlock_unlock(lock);
     }
     return NULL;
 }
 
 int main() {
     pthread_t thread1, thread2;
     spinlock_t lock;
 
     spinlock_init(&lock);
     shared_value = 0;
 
     printf("Shared value: %d\n", shared_value);
 
     if (pthread_create(&thread1, NULL, thread_function, &lock) == 0)
         printf("thread1 create success!\n");
     else {
         perror("thread1 create failed");
         exit(1);
     }
 
     if (pthread_create(&thread2, NULL, thread_function, &lock) == 0)
         printf("thread2 create success!\n");
     else {
         perror("thread2 create failed");
         exit(1);
     }
 
     pthread_join(thread1, NULL);
     pthread_join(thread2, NULL);
 
     printf("Shared value: %d\n", shared_value);
     return 0;
 }
 