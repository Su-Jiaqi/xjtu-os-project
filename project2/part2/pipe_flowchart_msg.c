// pipe_flowchart_msg.c
// 严格对应流程图：两个子进程分别写入一段消息，父进程读取并打印

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

static void die(const char *s) { perror(s); exit(EXIT_FAILURE); }

int main(void) {
    int fd[2];
    if (pipe(fd) == -1) die("pipe");

    pid_t pid1 = fork();
    if (pid1 < 0) die("fork child1");

    if (pid1 == 0) {
        // 子进程1：锁定写端 -> 写消息 -> 解锁 -> 退出
        close(fd[0]); // 只写
        if (lockf(fd[1], F_LOCK, 0) == -1) die("lockf child1 F_LOCK");
        const char *msg = "Child process 1 is sending message!\n";
        if (write(fd[1], msg, strlen(msg)) != (ssize_t)strlen(msg)) die("write child1");
        if (lockf(fd[1], F_ULOCK, 0) == -1) die("lockf child1 F_ULOCK");
        close(fd[1]);
        _exit(0);
    }

    pid_t pid2 = fork();
    if (pid2 < 0) die("fork child2");

    if (pid2 == 0) {
        // 子进程2：锁定写端 -> 写消息 -> 解锁 -> 退出
        close(fd[0]); // 只写
        if (lockf(fd[1], F_LOCK, 0) == -1) die("lockf child2 F_LOCK");
        const char *msg = "Child process 2 is sending message!\n";
        if (write(fd[1], msg, strlen(msg)) != (ssize_t)strlen(msg)) die("write child2");
        if (lockf(fd[1], F_ULOCK, 0) == -1) die("lockf child2 F_ULOCK");
        close(fd[1]);
        _exit(0);
    }

    // 父进程：只读
    close(fd[1]);

    char buf[1024];
    ssize_t n;
    while ((n = read(fd[0], buf, sizeof(buf))) > 0) {
        if (write(STDOUT_FILENO, buf, n) != n) die("write stdout");
    }
    if (n < 0) die("read parent");

    close(fd[0]);

    int st;
    waitpid(pid1, &st, 0);
    waitpid(pid2, &st, 0);
    return 0;
}
