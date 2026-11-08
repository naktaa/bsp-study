#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "mpu6050_data.h"

#define DEVICE_PATH "/dev/mpu6050"
#define SAMPLE_COUNT 20

/* 드라이버의 측정 범위 ±2g·±250°/s에 대응하는 감도다. */
#define ACCEL_LSB_PER_G 16384.0
#define GYRO_LSB_PER_DPS 131.0

static int mpu6050_read_sample(int fd, struct mpu6050_sample *sample)
{
    ssize_t received;

    do {
        received = read(fd, sample, sizeof(*sample));
    } while (received == -1 && errno == EINTR);

    if (received == -1)
        return -1;
    if ((size_t)received != sizeof(*sample)) {
        errno = EIO;
        return -1;
    }

    return 0;
}

int main(void)
{
    struct mpu6050_sample sample;
    __u32 previous_sequence = 0;
    unsigned int discontinuities = 0;
    int received = 0;
    int status = 0;

    printf("=== OPEN ===\n");

    int fd = open(DEVICE_PATH, O_RDONLY);

    if (fd == -1) {
        perror("[OPEN] " DEVICE_PATH);
        return 1;
    }

    printf("device: %s\n", DEVICE_PATH);
    printf("\n=== READ: %d FIFO samples, blocking read ===\n", SAMPLE_COUNT);
    printf("range: accel=+/-2 g, gyro=+/-250 deg/s\n\n");
    printf("%4s %10s %6s | %26s | %26s\n",
           "", "", "", "Acceleration (g)", "Angular velocity (deg/s)");
    printf("%4s %10s %6s | %8s %8s %8s | %8s %8s %8s\n",
           "No.", "Seq", "Skip", "X", "Y", "Z", "X", "Y", "Z");

    while (received < SAMPLE_COUNT) {
        if (mpu6050_read_sample(fd, &sample) == -1) {
            if (errno == EOVERFLOW) {
                discontinuities++;
                fprintf(stderr, "[READ] data discontinuity (FIFO/queue); resuming\n");
                continue;
            }

            perror("[READ] " DEVICE_PATH);
            status = 1;
            break;
        }

        unsigned int skipped = received == 0 ? 0 : sample.sequence - previous_sequence - 1U;

        printf("%4d %10u %6u | %+8.3f %+8.3f %+8.3f | %+8.3f %+8.3f %+8.3f\n",
               received + 1, sample.sequence, skipped,
               sample.accel[0] / ACCEL_LSB_PER_G,
               sample.accel[1] / ACCEL_LSB_PER_G,
               sample.accel[2] / ACCEL_LSB_PER_G,
               sample.gyro[0] / GYRO_LSB_PER_DPS,
               sample.gyro[1] / GYRO_LSB_PER_DPS,
               sample.gyro[2] / GYRO_LSB_PER_DPS);
        fflush(stdout);
        previous_sequence = sample.sequence;
        received++;
    }

    printf("\n=== RESULT ===\n");
    printf("samples: %d, discontinuities: %u\n", received, discontinuities);
    printf("\n=== CLOSE ===\n");

    if (close(fd) == -1) {
        perror("[CLOSE] " DEVICE_PATH);
        status = 1;
    }

    return status;
}
