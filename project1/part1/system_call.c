#include <stdio.h>
#include <unistd.h>

int main() {
    printf("system_call running in pid=%d\n", getpid());
    return 0;
}
