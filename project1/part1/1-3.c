#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>

int g = 100;  // 全局变量

int main() {
    pid_t pid = fork();

    if (pid == 0) {
        g += 50; // 子进程修改
        printf("child(pid=%d): g=%d\n", getpid(), g);
    } else {
        g -= 30; // 父进程修改
        printf("parent(pid=%d): g=%d\n", getpid(), g);
        wait(NULL);
    }
    return 0;
}