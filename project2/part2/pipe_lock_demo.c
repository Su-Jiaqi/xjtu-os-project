// pipe_lock_demo.c
// 父进程读，两子进程分别向匿名管道写 2000 个 '1'/'2'
// 支持 --lock（有锁）/ --nolock（无锁，默认）
// 可选 --yield/--sleep 用于更明显地演示无锁下的交错
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sched.h>   // sched_yield

#define WRITE_COUNT 2000
#define TOTAL_READ  (WRITE_COUNT * 2)

static void die(const char *msg) { perror(msg); exit(EXIT_FAILURE); }

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [--lock|--nolock] [--yield|--sleep]\n"
        "  --lock    : lockf(F_LOCK/F_ULOCK) 包围每个写者（互斥写）\n"
        "  --nolock  : 不加锁（默认）\n"
        "  --yield   : 每写 50 次 sched_yield() 便于观察交错\n"
        "  --sleep   : 每写 50 次 usleep(1000) 更明显的交错\n",
        prog);
}

int main(int argc, char *argv[]) {
    int use_lock  = 0;   // 默认无锁
    int use_yield = 0;
    int use_sleep = 0;

    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "--lock")   == 0) use_lock = 1;
        else if (strcmp(argv[i], "--nolock") == 0) use_lock = 0;
        else if (strcmp(argv[i], "--yield")  == 0) use_yield = 1;
        else if (strcmp(argv[i], "--sleep")  == 0) use_sleep = 1;
        else { usage(argv[0]); return 2; }
    }
    if (use_yield && use_sleep) { use_yield = 0; } // 若都给，优先 sleep

    int fd[2];
    if (pipe(fd) == -1) die("pipe");

    pid_t pid1 = fork();
    if (pid1 < 0) die("fork child1");
    if (pid1 == 0) {
        close(fd[0]); // 只写
        if (use_lock && lockf(fd[1], F_LOCK, 0) == -1) die("lockf child1 F_LOCK");
        char c = '1';
        for (int i = 0; i < WRITE_COUNT; i++) {
            if (write(fd[1], &c, 1) != 1) die("write child1");
            if ((i % 50) == 0) {
                if (use_sleep) usleep(1000);
                else if (use_yield) sched_yield();
            }
        }
        if (use_lock && lockf(fd[1], F_ULOCK, 0) == -1) die("lockf child1 F_ULOCK");
        close(fd[1]);
        _exit(0);
    }

    pid_t pid2 = fork();
    if (pid2 < 0) die("fork child2");
    if (pid2 == 0) {
        close(fd[0]); // 只写
        if (use_lock && lockf(fd[1], F_LOCK, 0) == -1) die("lockf child2 F_LOCK");
        char c = '2';
        for (int i = 0; i < WRITE_COUNT; i++) {
            if (write(fd[1], &c, 1) != 1) die("write child2");
            if ((i % 50) == 0) {
                if (use_sleep) usleep(1000);
                else if (use_yield) sched_yield();
            }
        }
        if (use_lock && lockf(fd[1], F_ULOCK, 0) == -1) die("lockf child2 F_ULOCK");
        close(fd[1]);
        _exit(0);
    }

    // 父进程：只读
    close(fd[1]);

    char *buf = (char *)malloc(TOTAL_READ + 1);
    if (!buf) die("malloc");
    int read_total = 0;
    while (read_total < TOTAL_READ) {
        ssize_t n = read(fd[0], buf + read_total, TOTAL_READ - read_total);
        if (n < 0) { if (errno == EINTR) continue; die("read parent"); }
        if (n == 0) break; // EOF
        read_total += (int)n;
    }
    buf[read_total] = '\0';

    int st;
    waitpid(pid1, &st, 0);
    waitpid(pid2, &st, 0);

    // 统计：数量 + 交错切换次数
    int cnt1 = 0, cnt2 = 0, switches = 0;
    for (int i = 0; i < read_total; i++) {
        if (buf[i] == '1') cnt1++;
        else if (buf[i] == '2') cnt2++;
        if (i > 0 && buf[i] != buf[i-1]) switches++;
    }

    printf("[mode=%s%s%s] read=%d, '1'=%d, '2'=%d, switches=%d\n",
           use_lock ? "LOCK" : "NOLOCK",
           use_yield ? "+YIELD" : "",
           use_sleep ? "+SLEEP" : "",
           read_total, cnt1, cnt2, switches);

    printf("head: %.120s%s\n", buf, (read_total > 120 ? "..." : ""));
    printf("tail: %.120s%s\n",
           read_total > 120 ? buf + read_total - 120 : buf,
           read_total > 120 ? "..." : "");

    close(fd[0]);
    free(buf);
    return 0;
}
