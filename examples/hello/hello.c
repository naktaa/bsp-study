#include <stdio.h>
#include <sys/utsname.h>

int main(void)
{
    struct utsname info;

    if (uname(&info) == -1) {
        perror("uname");
        return 1;
    }

    puts("Hello, BSP!");
    printf("Running on: %s\n", info.machine);
    printf("Kernel: %s\n", info.release);
    return 0;
}
