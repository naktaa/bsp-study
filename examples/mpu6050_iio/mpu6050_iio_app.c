#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <inttypes.h>
#include <limits.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define SCAN_BYTES 24
#define TIMESTAMP_OFFSET 16

static volatile sig_atomic_t stopping;

static void stop_capture(int signal_number)
{
    (void)signal_number;
    stopping = 1;
}

static const char *channels[] = {
    "in_accel_x", "in_accel_y", "in_accel_z",
    "in_anglvel_x", "in_anglvel_y", "in_anglvel_z",
    "in_timestamp",
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

static int write_attribute(const char *directory, const char *attribute,
                           const char *value)
{
    char path[PATH_MAX];
    FILE *file;
    int ret;

    if (snprintf(path, sizeof(path), "%s/%s", directory, attribute) >= (int)sizeof(path)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    file = fopen(path, "w");
    if (!file)
        return -1;
    ret = fputs(value, file);
    if (fclose(file) == EOF)
        return -1;
    return ret == EOF ? -1 : 0;
}

static int find_device(char *directory, size_t size)
{
    glob_t paths = {0};
    char name[64];
    unsigned int found = 0;

    if (glob("/sys/bus/iio/devices/iio:device*", 0, NULL, &paths)) {
        globfree(&paths);
        fprintf(stderr, "[CONFIG] load the MPU6050 IIO module and overlay first\n");
        return -1;
    }
    for (size_t i = 0; i < paths.gl_pathc; i++) {
        if (read_attribute(paths.gl_pathv[i], "name", name, sizeof(name)) ||
            strcmp(name, "mpu6050-iio-study"))
            continue;
        if (snprintf(directory, size, "%s", paths.gl_pathv[i]) >= (int)size) {
            globfree(&paths);
            return -1;
        }
        found++;
    }
    globfree(&paths);
    if (found != 1) {
        fprintf(stderr, "[CONFIG] expected one mpu6050-iio-study device, found %u\n", found);
        return -1;
    }
    return 0;
}

static int prepare_buffer(const char *directory)
{
    char attribute[128];

    for (unsigned int i = 0; i < sizeof(channels) / sizeof(channels[0]); i++) {
        snprintf(attribute, sizeof(attribute), "scan_elements/%s_en", channels[i]);
        if (write_attribute(directory, attribute, "1\n"))
            return -1;
    }
    if (write_attribute(directory, "buffer/length", "128\n"))
        return -1;
    return write_attribute(directory, "buffer/watermark", "1\n");
}

static int check_scan(const char *directory)
{
    char attribute[128];
    char value[64];

    for (unsigned int i = 0; i < sizeof(channels) / sizeof(channels[0]); i++) {
        snprintf(attribute, sizeof(attribute), "scan_elements/%s_index", channels[i]);
        if (read_attribute(directory, attribute, value, sizeof(value)) ||
            value[0] != (char)('0' + i) || value[1] != '\0')
            goto invalid;

        snprintf(attribute, sizeof(attribute), "scan_elements/%s_type", channels[i]);
        if (read_attribute(directory, attribute, value, sizeof(value)) ||
            strcmp(value, i == 6 ? "le:s64/64>>0" : "be:s16/16>>0"))
            goto invalid;
    }

    if (read_attribute(directory, "current_timestamp_clock", value, sizeof(value)) ||
        strcmp(value, "monotonic"))
        goto invalid;
    return 0;

invalid:
    fprintf(stderr, "[CONFIG] select six axes + timestamp (24 bytes), clock=monotonic\n");
    return -1;
}

static int read_sample(int fd, uint8_t raw[SCAN_BYTES])
{
    struct pollfd event = { .fd = fd, .events = POLLIN };
    ssize_t received;
    int ret;

    do {
        ret = poll(&event, 1, 5000);
    } while (ret < 0 && errno == EINTR && !stopping);
    if (stopping) {
        errno = EINTR;
        return -1;
    }
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
    } while (received < 0 && errno == EINTR && !stopping);
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

/* 파일명에는 현지 시각을 쓰고 CSV 내부에는 IRQ 시각을 그대로 기록한다. */
static FILE *open_log(const char *directory, char *path, size_t size)
{
    char name[64];
    struct tm local;
    time_t now = time(NULL);

    if (mkdir(directory, 0755) && errno != EEXIST)
        return NULL;
    if (now == (time_t)-1 || !localtime_r(&now, &local) ||
        !strftime(name, sizeof(name), "%Y%m%d-%H%M%S_mpu6050.csv", &local)) {
        errno = EINVAL;
        return NULL;
    }
    if (snprintf(path, size, "%s/%s", directory, name) >= (int)size) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    /* 같은 초에 실행한 파일이 있으면 덮어쓰지 않는다. */
    return fopen(path, "wx");
}

int main(int argc, char **argv)
{
    struct sigaction action = { .sa_handler = stop_capture };
    char attribute[64];
    char directory[PATH_MAX];
    char log_directory[PATH_MAX];
    char device_path[PATH_MAX];
    char log_path[PATH_MAX];
    uint8_t raw[SCAN_BYTES];
    int64_t timestamp;
    int64_t first_timestamp = 0;
    int64_t previous_timestamp = 0;
    int64_t last_flush = 0;
    int64_t last_display = 0;
    uint64_t samples = 0;
    double elapsed;
    double scale[2];
    double values[6];
    const char *device_name;
    const char *sudo_user;
    struct passwd *user;
    bool buffer_started = false;
    FILE *log;
    int status = 0;
    int fd;

    if (argc != 1) {
        fprintf(stderr, "usage: %s\n", argv[0]);
        return 1;
    }

    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL)) {
        perror("[SIGNAL]");
        return 1;
    }

    if (find_device(directory, sizeof(directory)))
        return 1;
    device_name = strrchr(directory, '/');
    if (check_scan(directory))
        return 1;

    if (read_attribute(directory, "buffer/enable", attribute, sizeof(attribute))) {
        perror("[BUFFER] read enable");
        return 1;
    }
    if (strcmp(attribute, "0")) {
        fprintf(stderr, "[BUFFER] already active; stop the other collector first\n");
        return 1;
    }
    if (prepare_buffer(directory)) {
        perror("[BUFFER] setup (check IIO access permissions)");
        return 1;
    }

    /* sudo로 실행해도 원래 사용자의 홈에 기록한다. */
    sudo_user = geteuid() == 0 ? getenv("SUDO_USER") : NULL;
    user = sudo_user ? getpwnam(sudo_user) : getpwuid(getuid());
    if (!user || snprintf(log_directory, sizeof(log_directory), "%s/bsp", user->pw_dir) >=
        (int)sizeof(log_directory)) {
        fprintf(stderr, "[LOG] cannot determine user home\n");
        return 1;
    }
    if (mkdir(log_directory, 0755) && errno != EEXIST) {
        perror("[LOG] create bsp directory");
        return 1;
    }
    if (snprintf(log_directory, sizeof(log_directory), "%s/bsp/logs", user->pw_dir) >=
        (int)sizeof(log_directory))
        return 1;

    if (read_attribute(directory, "in_accel_scale", attribute, sizeof(attribute)) ||
        sscanf(attribute, "%lf", &scale[0]) != 1 ||
        read_attribute(directory, "in_anglvel_scale", attribute, sizeof(attribute)) ||
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
    log = open_log(log_directory, log_path, sizeof(log_path));
    if (!log) {
        perror("[LOG] create (existing file is not overwritten)");
        close(fd);
        return 1;
    }
    if (fprintf(log, "time_s,ax_m_s2,ay_m_s2,az_m_s2,gx_rad_s,gy_rad_s,gz_rad_s\n") < 0 ||
        fflush(log)) {
        perror("[LOG] header");
        status = 1;
        goto out;
    }

    if (stopping)
        goto out;
    /* 읽기와 기록 준비를 마친 뒤 수집을 켜고, 실패 시에도 중지를 시도한다. */
    buffer_started = true;
    if (write_attribute(directory, "buffer/enable", "1\n")) {
        perror("[BUFFER] enable");
        status = 1;
        goto out;
    }

    printf("=== OPEN ===\ndevice: %s\nCSV: %s\n", device_path, log_path);
    printf("time: seconds since first IRQ sample\n");
    printf("\n=== CONTINUOUS READ: Ctrl+C to stop ===\n");
    printf("         |      Accel (m/s^2)       |       Gyro (rad/s)       |\n");
    printf(" Time(s) |       X       Y       Z |       X       Y       Z | Samples\n");
    fflush(stdout);

    while (!stopping) {
        if (read_sample(fd, raw)) {
            if (stopping && errno == EINTR)
                break;
            perror("[READ]");
            status = 1;
            break;
        }

        /* Pi ARM64의 timestamp는 리틀엔디언이며 정렬된 변수로 복사한다. */
        memcpy(&timestamp, &raw[TIMESTAMP_OFFSET], sizeof(timestamp));
        if (timestamp <= 0 || (samples && timestamp <= previous_timestamp)) {
            fprintf(stderr, "[TIME] timestamp did not advance\n");
            status = 1;
            break;
        }
        if (!samples)
            first_timestamp = timestamp;
        elapsed = (timestamp - first_timestamp) / 1000000000.0;
        for (int axis = 0; axis < 6; axis++)
            values[axis] = decode_axis(&raw[axis * 2]) * scale[axis / 3];

        if (fprintf(log, "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                    elapsed, values[0], values[1], values[2],
                    values[3], values[4], values[5]) < 0) {
            perror("[LOG] write");
            status = 1;
            break;
        }
        samples++;

        /* 기록이 사용자 버퍼에만 오래 머물지 않도록 약 100ms마다 비운다. */
        if (timestamp - last_flush >= 100000000LL) {
            if (fflush(log)) {
                perror("[LOG] flush");
                status = 1;
                break;
            }
            last_flush = timestamp;
        }
        if (timestamp - last_display >= 1000000000LL) {
            printf("%8.2f | %+7.2f %+7.2f %+7.2f | %+7.2f %+7.2f %+7.2f | %" PRIu64 "\n",
                   elapsed, values[0], values[1], values[2],
                   values[3], values[4], values[5], samples);
            fflush(stdout);
            last_display = timestamp;
        }
        previous_timestamp = timestamp;
    }

out:
    if (buffer_started && write_attribute(directory, "buffer/enable", "0\n")) {
        perror("[BUFFER] disable");
        status = 1;
    }
    printf("\n=== CLOSE ===\nsamples: %" PRIu64 "\n", samples);
    if (fclose(log)) {
        perror("[LOG] close");
        status = 1;
    }
    if (close(fd)) {
        perror("[CLOSE]");
        status = 1;
    }
    return status;
}
