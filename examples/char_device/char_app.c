#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

#include "char_ioctl.h"

#define DEVICE_PATH "/dev/char_drv"

int main(void)
{
    const char message[] = "Hello, driver!";
    size_t count = sizeof(message) - 1;
    int status = 0;

    printf("=== OPEN ===\n");

    int fd = open(DEVICE_PATH, O_RDWR);

    if (fd == -1) {
        perror("[OPEN] " DEVICE_PATH);
        return 1;
    }

    printf("device: %s\nfd: %d\n", DEVICE_PATH, fd);

    printf("\n=== WRITE ===\n");

    ssize_t written = write(fd, message, count);

    if (written == -1) {
        perror("[WRITE] " DEVICE_PATH);
        status = 1;
    } else if ((size_t)written != count) {
        fprintf(stderr, "[WRITE] short write: %zd/%zu bytes\n", written, count);
        status = 1;
    } else {
        printf("sent: %zd bytes\n", written);
    }

    char reply[64];
    char chunk[32];
    size_t total = 0;
    ssize_t received;

    printf("\n=== READ ===\n");

    while ((received = read(fd, chunk, sizeof(chunk))) > 0) {
        if ((size_t)received > sizeof(reply) - 1 - total) {
            fprintf(stderr, "[READ] reply too long\n");
            status = 1;
            break;
        }

        memcpy(reply + total, chunk, (size_t)received);
        total += (size_t)received;
        printf("received: %zd bytes\n", received);
    }

    if (received == -1) {
        perror("[READ] " DEVICE_PATH);
        status = 1;
    } else if (received == 0) {
        printf("EOF\n");
    }

    reply[total] = '\0';
    printf("reply: %s\n", reply);

    if (status == 0) {
        printf("\n=== VERIFY ===\n");

        if (total != count || memcmp(reply, message, count) != 0) {
            fprintf(stderr, "[VERIFY] echo mismatch\n");
            status = 1;
        } else {
            printf("echo matched: %zu bytes\n", total);
        }
    }

    if (status == 0) {
        printf("\n=== IOCTL: CLEAR ===\n");

        if (ioctl(fd, CHAR_CLEAR) == -1) {
            perror("[IOCTL: CLEAR] ioctl");
            status = 1;
        } else {
            printf("buffer cleared\n");

            received = read(fd, chunk, sizeof(chunk));
            if (received == -1) {
                perror("[IOCTL: CLEAR] read after clear");
                status = 1;
            } else if (received != 0) {
                fprintf(stderr, "[IOCTL: CLEAR] expected EOF after clear, got %zd bytes\n",
                        received);
                status = 1;
            } else {
                printf("read after clear: EOF\n");
            }
        }
    }

    printf("\n=== CLOSE ===\n");

    if (close(fd) == -1) {
        perror("[CLOSE] " DEVICE_PATH);
        return 1;
    }

    printf("device closed\n");

    return status;
}
