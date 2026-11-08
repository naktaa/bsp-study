#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SAMPLE_COUNT 20
#define SCAN_BYTES 12

static const char *channels[] = {
    "in_accel_x", "in_accel_y", "in_accel_z",
    "in_anglvel_x", "in_anglvel_y", "in_anglvel_z",
};

static int read_attribute(const char *directory, const char *attribute,
                          char *value, size_t size)
{
    char path[PATH_MAX];
    FILE *file;

    if (snprintf(path, sizeof(path), "%s/%s", directory, attribute) >= (int)sizeof(path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    file = fopen(path, "r");
    if (!file)
        return -1;
    if (!fgets(value, size, file)) {
        fclose(file);
        errno = EIO;
        return -1;
    }
    if (fclose(file) == EOF)
        return -1;

    value[strcspn(value, "\n")] = '\0';
    return 0;
}

static int check_scan(const char *directory)
{
    char attribute[128];
    char value[64];

    for (unsigned int i = 0; i < sizeof(channels) / sizeof(channels[0]); i++) {
        snprintf(attribute, sizeof(attribute), "scan_elements/%s_en", channels[i]);
        if (read_attribute(directory, attribute, value, sizeof(value)) || strcmp(value, "1"))
            goto invalid;

        snprintf(attribute, sizeof(attribute), "scan_elements/%s_index", channels[i]);
        if (read_attribute(directory, attribute, value, sizeof(value)) ||
            value[0] != (char)('0' + i) || value[1] != '\0')
            goto invalid;

        snprintf(attribute, sizeof(attribute), "scan_elements/%s_type", channels[i]);
        if (read_attribute(directory, attribute, value, sizeof(value)) ||
            strcmp(value, "be:s16/16>>0"))
            goto invalid;
    }

    if (read_attribute(directory, "buffer/enable", value, sizeof(value)) || strcmp(value, "1"))
        goto invalid;
    return 0;

invalid:
    fprintf(stderr, "[CONFIG] enable all six channels in the documented 12-byte format\n");
    return -1;
}

static int read_sample(int fd, uint8_t raw[SCAN_BYTES])
{
    struct pollfd event = { .fd = fd, .events = POLLIN };
    ssize_t received;
    int ret;

    do {
        ret = poll(&event, 1, 5000);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0)
        return -1;
    if (!ret) {
        errno = ETIMEDOUT;
        return -1;
    }
    if (event.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        errno = ENODEV;
        return -1;
    }

    do {
        received = read(fd, raw, SCAN_BYTES);
    } while (received < 0 && errno == EINTR);
    if (received < 0)
        return -1;
    if (received != SCAN_BYTES) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static int16_t decode_axis(const uint8_t *raw)
{
    return (int16_t)(((uint16_t)raw[0] << 8) | raw[1]);
}

int main(int argc, char **argv)
{
    char attribute[64];
    char device_path[PATH_MAX];
    uint8_t raw[SCAN_BYTES];
    double scale[2];
    const char *device_name;
    int status = 0;
    int fd;

    if (argc != 2) {
        fprintf(stderr, "usage: %s /sys/bus/iio/devices/iio:deviceN\n", argv[0]);
        return 1;
    }

    device_name = strrchr(argv[1], '/');
    if (!device_name || strncmp(device_name + 1, "iio:device", 10)) {
        fprintf(stderr, "[CONFIG] pass the IIO sysfs directory without a trailing slash\n");
        return 1;
    }
    if (read_attribute(argv[1], "name", attribute, sizeof(attribute)) ||
        strcmp(attribute, "mpu6050-iio-study")) {
        fprintf(stderr, "[CONFIG] MPU6050 IIO device not found\n");
        return 1;
    }
    if (check_scan(argv[1]))
        return 1;

    if (read_attribute(argv[1], "in_accel_scale", attribute, sizeof(attribute)) ||
        sscanf(attribute, "%lf", &scale[0]) != 1 ||
        read_attribute(argv[1], "in_anglvel_scale", attribute, sizeof(attribute)) ||
        sscanf(attribute, "%lf", &scale[1]) != 1) {
        fprintf(stderr, "[CONFIG] failed to read scale\n");
        return 1;
    }

    if (snprintf(device_path, sizeof(device_path), "/dev/%s", device_name + 1) >=
        (int)sizeof(device_path))
        return 1;
    fd = open(device_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("[OPEN]");
        return 1;
    }

    printf("=== OPEN ===\ndevice: %s\n", device_path);
    printf("\n=== READ: %d IIO scans ===\n", SAMPLE_COUNT);
    printf(" No. |       Acceleration (m/s^2) |   Angular velocity (rad/s)\n");
    printf("     |        X        Y        Z |        X        Y        Z\n");

    for (int i = 0; i < SAMPLE_COUNT; i++) {
        if (read_sample(fd, raw)) {
            perror("[READ]");
            status = 1;
            break;
        }

        printf("%4d | %+8.3f %+8.3f %+8.3f | %+8.3f %+8.3f %+8.3f\n",
               i + 1,
               decode_axis(&raw[0]) * scale[0],
               decode_axis(&raw[2]) * scale[0],
               decode_axis(&raw[4]) * scale[0],
               decode_axis(&raw[6]) * scale[1],
               decode_axis(&raw[8]) * scale[1],
               decode_axis(&raw[10]) * scale[1]);
    }

    printf("\n=== CLOSE ===\n");
    if (close(fd)) {
        perror("[CLOSE]");
        status = 1;
    }
    return status;
}
