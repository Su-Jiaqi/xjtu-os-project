// softint.c
// 实验二 2.1 进程的软中断通信（带信号编号打印版）
// 编译：
//   跨平台安全，发送 SIGUSR1/SIGUSR2：
//     gcc -std=c99 -O2 -Wall -Wextra -o softint softint.c
//   如课程要求“发 16 与 17 号信号”
//     gcc -std=c99 -O2 -Wall -Wextra -DUSE_RAW_16_17 -o softint softint.c
//
// 运行：./softint
//   - 5 秒内按 Ctrl+C 触发 SIGINT；按 Ctrl+\ 触发 SIGQUIT；
//   - 若 5 秒不操作，则 SIGALRM 触发；
//   - 触发后会打印三行编号：<触发信号>、<发给子2>、<发给子1>，然后子进程与父进程收尾。

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

#ifdef USE_RAW_16_17
  #define CHILD1_SIG 16
  #define CHILD2_SIG 17
  #define CHILD1_SIG_NAME "SIG(16)"
  #define CHILD2_SIG_NAME "SIG(17)"
#else
  #define CHILD1_SIG SIGUSR1
  #define CHILD2_SIG SIGUSR2
  #define CHILD1_SIG_NAME "SIGUSR1"
  #define CHILD2_SIG_NAME "SIGUSR2"
#endif

static volatile sig_atomic_t parent_tripped = 0;   // 父进程已触发
static volatile sig_atomic_t last_parent_sig = 0;  // 父进程触发信号编号
static volatile sig_atomic_t child1_got = 0;       // 子1收到用户信号
static volatile sig_atomic_t child2_got = 0;       // 子2收到用户信号

static void parent_trigger_handler(int sig) {
    last_parent_sig = sig;     // 记录 SIGINT / SIGQUIT / SIGALRM
    parent_tripped = 1;
}

static void child1_handler(int sig) { (void)sig; child1_got = 1; }
static void child2_handler(int sig) { (void)sig; child2_got = 1; }

static void set_handler(int signo, void (*fn)(int)) {
    struct sigaction sa = {0};
    sa.sa_handler = fn;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(signo, &sa, NULL) == -1) { perror("sigaction"); exit(EXIT_FAILURE); }
}

static void ignore_signal(int signo) {
    struct sigaction sa = {0};
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    if (sigaction(signo, &sa, NULL) == -1) { perror("sigaction ignore"); exit(EXIT_FAILURE); }
}

// 等待某个信号处理器置位 flag
static void wait_flag(volatile sig_atomic_t *flag) {
    sigset_t mask, old;
    sigemptyset(&mask);
    sigprocmask(SIG_SETMASK, &mask, &old);
    while (!*flag) pause();
    sigprocmask(SIG_SETMASK, &old, NULL);
}

int main(void) {
    int ready_pipe[2];
    if (pipe(ready_pipe) == -1) { perror("pipe"); return 1; }

    // 父进程安装触发器：Ctrl+C(SIGINT)、Ctrl+\(SIGQUIT)、5s 超时(SIGALRM)
    set_handler(SIGINT,  parent_trigger_handler);
    set_handler(SIGQUIT, parent_trigger_handler);
    set_handler(SIGALRM, parent_trigger_handler);
    alarm(5);

    pid_t pid1 = fork();
    if (pid1 < 0) { perror("fork1"); return 1; }

    if (pid1 == 0) {
        // 子进程 1：只关心来自父进程的“用户信号”，忽略 Ctrl+C / Ctrl+\\
        close(ready_pipe[0]); // 只写
        ignore_signal(SIGINT);
        ignore_signal(SIGQUIT);
        set_handler(CHILD1_SIG, child1_handler);

        (void)!write(ready_pipe[1], "1", 1); // 报告“已就绪”
        close(ready_pipe[1]);

        wait_flag(&child1_got);
        printf("Child process 1 is killed by parent !!\n");
        _exit(0);
    }

    pid_t pid2 = fork();
    if (pid2 < 0) { perror("fork2"); return 1; }

    if (pid2 == 0) {
        // 子进程 2
        close(ready_pipe[0]); // 只写
        ignore_signal(SIGINT);
        ignore_signal(SIGQUIT);
        set_handler(CHILD2_SIG, child2_handler);

        (void)!write(ready_pipe[1], "2", 1);
        close(ready_pipe[1]);

        wait_flag(&child2_got);
        printf("Child process 2 is killed by parent !!\n");
        _exit(0);
    }

    // 父进程：等待两个子进程都“准备好”
    close(ready_pipe[1]);
    char buf[2];
    ssize_t got = 0;
    while (got < 2) {
        ssize_t n = read(ready_pipe[0], buf + got, (size_t)(2 - got));
        if (n > 0) got += n;
        else if (n == -1 && errno == EINTR) continue;
        else break;
    }
    close(ready_pipe[0]);

    // 等待触发（Ctrl+C / Ctrl+\ / 5s 超时）
    wait_flag(&parent_tripped);

    // —— 打印与他人示例一致的三行编号 —— //
    // 1) 父进程接收到的触发信号编号（如 3 = SIGQUIT, 2 = SIGINT, 14 = SIGALRM）
    printf("%d stop test\n\n", (int)last_parent_sig);

    // 2) 按“先 17 后 16”的顺序给子进程发信号并打印
    kill(pid2, CHILD2_SIG);
    printf("%d stop test\n\n", (int)CHILD2_SIG);

    kill(pid1, CHILD1_SIG);
    printf("%d stop test\n\n", (int)CHILD1_SIG);

    // 等待两个子进程结束
    int status;
    waitpid(pid1, &status, 0);
    waitpid(pid2, &status, 0);

    printf("Parent process is killed!!  (sent %s and %s)\n",
           CHILD1_SIG_NAME, CHILD2_SIG_NAME);
    return 0;
}
