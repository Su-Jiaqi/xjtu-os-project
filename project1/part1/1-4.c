#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>

int g = 100;

int main() {
    pid_t pid = fork();

    if (pid == 0) {
        g += 50;
        printf("child-before-return(pid=%d): g=%d\n", getpid(), g);
        g *= 2;  // return 前再次操作
        printf("child-before-exit(pid=%d): g=%d\n", getpid(), g);
    } else {
        g -= 30;
        printf("parent-after-fork(pid=%d): g=%d\n", getpid(), g);
        wait(NULL);
        g += 1;  // return 前操作
        printf("parent-before-exit(pid=%d): g=%d\n", getpid(), g);
    }
    return 0;
}