// SPDX-License-Identifier: GPL-2.0-only
#include <alsa/asoundlib.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <inttypes.h>
#include <limits.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/media.h>
#include <linux/videodev2.h>
#include <math.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int read_attribute(const char *directory, const char *attribute, char value[64])
{
	char path[PATH_MAX];
	FILE *file;
	int ret = 0;

	snprintf(path, sizeof(path), "%s/%s", directory, attribute);
	file = fopen(path, "r");
	if (!file)
		return -1;

	if (fscanf(file, "%63s", value) != 1) {
		errno = EIO;
		ret = -1;
	}
	if (fclose(file))
		ret = -1;

	return ret;
}

static int write_attribute(const char *directory, const char *attribute, const char *value)
{
	char path[PATH_MAX];
	FILE *file;
	int ret;

	snprintf(path, sizeof(path), "%s/%s", directory, attribute);
	file = fopen(path, "w");
	if (!file)
		return -1;

	ret = fprintf(file, "%s\n", value) < 0 ? -1 : 0;
	if (fclose(file))
		ret = -1;

	return ret;
}

static int report(const char *device, int ret, const char *reason)
{
	if (ret)
		printf("%s: FAIL %s (%s)\n", device, reason, strerror(-ret));
	else
		printf("%s: PASS %s\n", device, reason);

	return ret ? 1 : 0;
}

static const char *path_name(const char *path)
{
	return strrchr(path, '/') + 1;
}

static int find_iio(const char *name, char directory[PATH_MAX])
{
	glob_t paths = { 0 };
	char value[64];
	size_t i;
	int ret = -1;

	glob("/sys/bus/iio/devices/iio:device*", 0, NULL, &paths);
	for (i = 0; i < paths.gl_pathc; i++) {
		if (!read_attribute(paths.gl_pathv[i], "name", value) && !strcmp(value, name)) {
			snprintf(directory, PATH_MAX, "%s", paths.gl_pathv[i]);
			ret = 0;
			break;
		}
	}
	globfree(&paths);
	if (ret)
		errno = ENODEV;

	return ret;
}

static int find_pwm(bool pca, char directory[PATH_MAX])
{
	const char *pattern = pca ?
		"/sys/bus/i2c/drivers/pca9685-pwm/1-0040/pwm/pwmchip*" :
		"/sys/bus/platform/drivers/bcm2835-pwm/*.pwm/pwm/pwmchip*";
	glob_t paths = { 0 };
	int ret = -1;

	glob(pattern, 0, NULL, &paths);
	if (paths.gl_pathc) {
		snprintf(directory, PATH_MAX, "%s", paths.gl_pathv[0]);
		ret = 0;
	}
	globfree(&paths);
	if (ret)
		errno = ENODEV;

	return ret;
}

static int check_imu(void)
{
	const char * const attributes[] = {
		"scan_elements/in_accel_x_en", "scan_elements/in_accel_y_en",
		"scan_elements/in_accel_z_en", "scan_elements/in_anglvel_x_en",
		"scan_elements/in_anglvel_y_en", "scan_elements/in_anglvel_z_en",
		"scan_elements/in_timestamp_en", "buffer/length", "buffer/watermark",
	};
	char directory[PATH_MAX], driver[PATH_MAX], node[PATH_MAX + 5];
	char saved[9][64], value[64];
	struct pollfd event = { .events = POLLIN };
	uint8_t raw[24];
	uint64_t encoded;
	int64_t timestamp, previous = 0;
	double minimum = 0, maximum = 0, sum = 0;
	bool changed = false;
	int ret = 0, fd = -1, ready;
	unsigned int i;
	ssize_t length;

	if (!realpath("/sys/bus/i2c/devices/1-0068/driver", driver))
		return report("imu", -errno, "read 1-0068/driver");

	printf("imu: driver=%s", path_name(driver));
	if (find_iio("mpu6050-iio-study", directory) ||
	    read_attribute(directory, "buffer/enable", value))
		goto fail;
	if (strcmp(value, "0")) {
		errno = EBUSY;
		goto fail;
	}

	printf(" iio=%s", path_name(directory));
	for (i = 0; i < 9; i++) {
		if (read_attribute(directory, attributes[i], saved[i]))
			goto fail;
	}

	changed = true;
	for (i = 0; i < 9; i++) {
		if (write_attribute(directory, attributes[i], i == 7 ? "128" : "1"))
			goto fail;
	}

	snprintf(node, sizeof(node), "/dev/%s", path_name(directory));
	fd = open(node, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0 || write_attribute(directory, "buffer/enable", "1"))
		goto fail;

	event.fd = fd;
	for (i = 0; i < 20; i++) {
		double interval;

		ready = poll(&event, 1, 5000);
		if (ready <= 0) {
			if (!ready)
				errno = ETIMEDOUT;
			goto fail;
		}

		length = read(fd, raw, sizeof(raw));
		if (length != sizeof(raw)) {
			if (length >= 0)
				errno = EIO;
			goto fail;
		}

		/* 우리 드라이버의 고정 scan은 24B이며 timestamp offset은 16B다. */
		memcpy(&encoded, raw + 16, sizeof(encoded));
		timestamp = le64toh(encoded);
		if (timestamp <= previous) {
			errno = ERANGE;
			goto fail;
		}

		if (i) {
			interval = (timestamp - previous) / 1000000.0;
			minimum = i == 1 ? interval : fmin(minimum, interval);
			maximum = fmax(maximum, interval);
			sum += interval;
		}
		previous = timestamp;
	}
	printf(" samples=20 dt_avg_ms=%.3f dt_min_ms=%.3f dt_max_ms=%.3f",
	       sum / 19, minimum, maximum);
	goto out;

fail:
	ret = -errno;
out:
	if (changed) {
		if (write_attribute(directory, "buffer/enable", "0"))
			ret = -errno;

		for (i = 0; i < 9; i++) {
			if (write_attribute(directory, attributes[i], saved[i]))
				ret = -errno;
		}
	}
	if (fd >= 0 && close(fd) && !ret)
		ret = -errno;

	printf("\n");
	return report("imu", ret, ret ? "capture/restore (buffer must be idle)" :
		      "20 samples; settings restored");
}

static int read_pca(void)
{
	const uint8_t registers[] = { 0x00, 0xfe, 0x06, 0x07, 0x08, 0x09 };
	uint8_t values[6];
	union i2c_smbus_data data;
	struct i2c_smbus_ioctl_data request = {
		.read_write = I2C_SMBUS_READ, .size = I2C_SMBUS_BYTE_DATA, .data = &data,
	};
	unsigned int i;
	int fd, ret = 0;

	fd = open("/dev/i2c-1", O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;

	/* 바인딩된 드라이버의 상태 관측에 FORCE를 쓰며 SMBus READ만 수행한다. */
	ret = ioctl(fd, I2C_SLAVE_FORCE, 0x40);
	for (i = 0; !ret && i < sizeof(registers); i++) {
		request.command = registers[i];
		ret = ioctl(fd, I2C_SMBUS, &request);
		if (!ret)
			values[i] = data.byte;
	}
	if (close(fd) && !ret)
		ret = -1;

	if (!ret)
		printf(" MODE1=0x%02x PRE_SCALE=0x%02x LED0=0x%02x,0x%02x,0x%02x,0x%02x",
		       values[0], values[1], values[2], values[3], values[4], values[5]);
	return ret;
}

static int check_pwm(bool pca)
{
	const char *name = pca ? "pca" : "pwm";
	const char *period = pca ? "20000000" : "1000000";
	const char *duty = pca ? "1500000" : "500000";
	char directory[PATH_MAX], channel[PATH_MAX + 16], number[16];
	char actual_period[64], actual_duty[64], enabled[64];
	unsigned int i, count = pca ? 1 : 2, owned = 0;
	int ret = 0;

	if (find_pwm(pca, directory))
		return report(name, -errno, "find pwmchip by device/driver");

	printf("%s: pwmchip=%s", name, path_name(directory));
	for (i = 0; i < count; i++) {
		snprintf(number, sizeof(number), "%u", i);
		/* 기존 export 채널은 커널이 거부하며, 직접 export한 채널만 정리한다. */
		if (write_attribute(directory, "export", number))
			goto fail;
		owned++;

		snprintf(channel, sizeof(channel), "%s/pwm%u", directory, i);
		if (read_attribute(channel, "period", actual_period))
			goto fail;

		/* 초기 period=0이면 enable/duty 쓰기도 거부되므로 period부터 설정한다. */
		if (strcmp(actual_period, "0") &&
		    (write_attribute(channel, "enable", "0") ||
		     write_attribute(channel, "duty_cycle", "0")))
			goto fail;

		if (write_attribute(channel, "period", period) ||
		    write_attribute(channel, "duty_cycle", duty) ||
		    write_attribute(channel, "enable", "1") ||
		    read_attribute(channel, "period", actual_period) ||
		    read_attribute(channel, "duty_cycle", actual_duty) ||
		    read_attribute(channel, "enable", enabled))
			goto fail;

		printf(" ch%u_period_ns=%s ch%u_duty_ns=%s ch%u_enable=%s",
		       i, actual_period, i, actual_duty, i, enabled);
		if (strcmp(enabled, "1")) {
			errno = EIO;
			goto fail;
		}
	}
	if (pca && read_pca())
		goto fail;
	goto out;

fail:
	ret = -errno;
out:
	for (i = 0; i < owned; i++) {
		snprintf(channel, sizeof(channel), "%s/pwm%u", directory, i);
		if (write_attribute(channel, "enable", "0"))
			ret = -errno;

		snprintf(number, sizeof(number), "%u", i);
		if (write_attribute(directory, "unexport", number))
			ret = -errno;
	}
	printf("\n");
	return report(name, ret, ret ? "configure/read/cleanup (channels must be unexported)" :
		      "readback OK; disabled/unexported");
}

static int check_sonar(void)
{
	char directory[PATH_MAX], value[64];
	unsigned int i;
	int ret = 0;

	if (find_iio("srf04", directory))
		return report("sonar", -errno, "find srf04");

	printf("sonar: iio=%s", path_name(directory));
	for (i = 0; i < 5; i++) {
		if (read_attribute(directory, "in_distance_raw", value)) {
			ret = -errno;
			break;
		}

		printf(" raw%u=%s", i + 1, value);
		if (i < 4)
			usleep(100000);
	}
	printf("\n");
	return report("sonar", ret, ret ? "read in_distance_raw" : "5 distance reads");
}

static int check_mic(void)
{
	snd_pcm_t *pcm = NULL;
	snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t period = 1024, buffer = 4096;
	unsigned int rate = 48000, frames = 0, xruns = 0, i;
	int32_t input[1024 * 2];
	char line[256], device[64];
	double squares = 0;
	int card = -1, ret = -ENODEV;
	FILE *cards;

	cards = fopen("/proc/asound/cards", "r");
	if (!cards)
		return report("mic", -errno, "read /proc/asound/cards");

	while (fgets(line, sizeof(line), cards)) {
		if (strstr(line, "[inmp441study ")) {
			card = strtol(line, NULL, 10);
			ret = 0;
			break;
		}
	}
	if (ferror(cards))
		ret = -EIO;
	if (fclose(cards) && !ret)
		ret = -errno;
	if (ret)
		return report("mic", ret, "find inmp441study");

	printf("mic: card=%d id=inmp441study", card);
	snprintf(device, sizeof(device), "hw:%d,0", card);
	ret = snd_pcm_open(&pcm, device, SND_PCM_STREAM_CAPTURE, 0);
	if (ret < 0)
		goto out;

	snd_pcm_hw_params_alloca(&params);
	if (snd_pcm_hw_params_any(pcm, params) < 0 ||
	    snd_pcm_hw_params_set_access(pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0 ||
	    snd_pcm_hw_params_set_format(pcm, params, SND_PCM_FORMAT_S32_LE) < 0 ||
	    snd_pcm_hw_params_set_channels(pcm, params, 2) < 0 ||
	    snd_pcm_hw_params_set_rate_near(pcm, params, &rate, NULL) < 0 ||
	    snd_pcm_hw_params_set_period_size_near(pcm, params, &period, NULL) < 0 ||
	    snd_pcm_hw_params_set_buffer_size_near(pcm, params, &buffer) < 0 ||
	    snd_pcm_hw_params(pcm, params) < 0) {
		ret = -EIO;
		goto out;
	}

	while (frames < rate) {
		snd_pcm_uframes_t wanted = rate - frames;
		snd_pcm_sframes_t received;

		if (wanted > 1024)
			wanted = 1024;
		received = snd_pcm_readi(pcm, input, wanted);
		if (received == -EPIPE) {
			xruns++;
			ret = snd_pcm_prepare(pcm);
			if (ret < 0)
				goto out;
			if (xruns == 5) {
				ret = -EPIPE;
				goto out;
			}
			continue;
		}
		if (received <= 0) {
			ret = received ? received : -EIO;
			goto out;
		}

		for (i = 0; i < (unsigned int)received; i++) {
			/* 기존 앱과 같이 왼쪽 채널의 상위 24비트로 RMS를 구한다. */
			int32_t sample = (int32_t)le32toh(input[i * 2]) >> 8;

			squares += (double)sample * sample;
		}
		frames += received;
	}
	ret = 0;

out:
	if (pcm) {
		int err = snd_pcm_close(pcm);

		if (err < 0 && !ret)
			ret = err;
	}
	printf(" rate=%u format=S32_LE channels=2 period=%lu buffer=%lu frames=%u rms=%.1f xrun=%u\n",
	       rate, period, buffer, frames, frames ? sqrt(squares / frames) : 0, xruns);
	return report("mic", ret, ret ? "ALSA capture" : "one second captured");
}

static int check_cam(void)
{
	const char *photo = "/tmp/devcheck-cam.jpg";
	glob_t paths = { 0 };
	struct media_entity_desc entity;
	struct v4l2_capability cap;
	struct stat info;
	unsigned char magic[2];
	bool sensor = false, unicam = false, codec = false;
	size_t i;
	int fd, status, ret, node_ret;
	ssize_t length;
	pid_t child;

	printf("cam:");
	glob("/dev/*", 0, NULL, &paths);
	for (i = 0; i < paths.gl_pathc; i++) {
		bool media = !strncmp(path_name(paths.gl_pathv[i]), "media", 5);
		bool video = !strncmp(path_name(paths.gl_pathv[i]), "video", 5);

		if (!media && !video)
			continue;

		fd = open(paths.gl_pathv[i], O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0)
			continue;

		if (media) {
			memset(&entity, 0, sizeof(entity));
			entity.id = MEDIA_ENT_ID_FLAG_NEXT;
			while (!ioctl(fd, MEDIA_IOC_ENUM_ENTITIES, &entity)) {
				if (strstr(entity.name, "imx219")) {
					printf(" media=%s entity=\"%.32s\"", paths.gl_pathv[i], entity.name);
					sensor = true;
				}
				entity.id |= MEDIA_ENT_ID_FLAG_NEXT;
			}
		} else {
			memset(&cap, 0, sizeof(cap));
			if (!ioctl(fd, VIDIOC_QUERYCAP, &cap) &&
			    (strstr((const char *)cap.driver, "unicam") ||
			     strstr((const char *)cap.driver, "bcm2835-codec"))) {
				unicam |= strstr((const char *)cap.driver, "unicam") != NULL;
				codec |= strstr((const char *)cap.driver, "bcm2835-codec") != NULL;
				printf(" video=%s driver=\"%.16s\" card=\"%.32s\"",
				       paths.gl_pathv[i], cap.driver, cap.card);
			}
		}
		close(fd);
	}
	globfree(&paths);
	printf(" imx219=%u unicam=%u codec=%u\n", sensor, unicam, codec);
	node_ret = sensor && unicam && codec ? 0 : -ENODEV;

	fflush(stdout);
	child = fork();
	if (child < 0)
		return report("cam", -errno, "fork rpicam-still");
	if (!child) {
		execlp("rpicam-still", "rpicam-still", "-n", "--immediate",
		       "-o", photo, (char *)NULL);
		_exit(127);
	}

	if (waitpid(child, &status, 0) < 0)
		return report("cam", -errno, "wait rpicam-still");
	if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
		printf("cam: capture=SKIP reason=rpicam-still-unavailable\n");
		return report("cam", node_ret, node_ret ? "recognize camera nodes" :
			      "recognized; capture SKIP");
	}

	printf("cam: capture_exit=%d signal=%d", WIFEXITED(status) ?
	       WEXITSTATUS(status) : -1, WIFSIGNALED(status) ? WTERMSIG(status) : 0);
	ret = -EIO;
	if (!WIFEXITED(status) || WEXITSTATUS(status))
		goto out;

	fd = open(photo, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		ret = -errno;
		goto out;
	}

	length = read(fd, magic, sizeof(magic));
	ret = fstat(fd, &info) < 0 || length < 0 ? -EIO : 0;
	if (!ret) {
		printf(" bytes=%jd FFD8=%u", (intmax_t)info.st_size,
		       length == 2 && magic[0] == 0xff && magic[1] == 0xd8);
		ret = length == 2 && info.st_size >= 2 && magic[0] == 0xff && magic[1] == 0xd8 ?
		      node_ret : -EINVAL;
	}
	if (close(fd) && !ret)
		ret = -errno;

out:
	printf("\n");
	return report("cam", ret, ret ? "recognition/capture/JPEG check" : "recognized; JPEG captured");
}

int main(int argc, char **argv)
{
	const char * const commands[] = { "imu", "pca", "cam", "mic", "sonar", "pwm" };
	size_t i;
	int status = 0;
	bool all;

	if (argc != 2)
		goto usage;

	all = !strcmp(argv[1], "all");
	for (i = 0; i < sizeof(commands) / sizeof(*commands); i++) {
		if (!strcmp(argv[1], commands[i]))
			break;
	}
	if (!all && i == sizeof(commands) / sizeof(*commands))
		goto usage;

	if (all || !strcmp(argv[1], "imu"))
		status |= check_imu();
	if (all || !strcmp(argv[1], "pca"))
		status |= check_pwm(true);
	if (all || !strcmp(argv[1], "cam"))
		status |= check_cam();
	if (all || !strcmp(argv[1], "mic"))
		status |= check_mic();
	if (all || !strcmp(argv[1], "sonar"))
		status |= check_sonar();
	if (all || !strcmp(argv[1], "pwm"))
		status |= check_pwm(false);

	return status;
usage:
	fprintf(stderr, "usage: %s all|imu|pca|cam|mic|sonar|pwm\n", argv[0]);
	return 1;
}
