// exec_helper.c
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

int main(int argc, char** argv) {
    printf("[exec_helper] hello. my PID=%d, PPID=%d. argc=%d\n", getpid(), getppid(), argc);
    for (int i = 0; i < argc; i++) {
        printf("[exec_helper] argv[%d]=%s\n", i, argv[i] ? argv[i] : "(null)");
    }
    return 0;
}
