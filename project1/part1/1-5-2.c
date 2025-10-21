#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(void) {
    pid_t pid = fork();

    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {
        printf("child process1 PID: %d\n", getpid());
        execl("./system_call", "system_call", (char *)NULL);
        perror("execl failed");   
        _exit(127);
    } else {
        printf("parent process PID: %d\n", getpid());
        wait(NULL);
        return 0;
    }
}
