// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static int video_ioctl(int fd, unsigned long request, void *arg,
		       const char *name)
{
	int ret = ioctl(fd, request, arg);

	if (ret < 0)
		perror(name);
	return ret;
}

int main(int argc, char **argv)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	struct v4l2_format fmt = { .type = type };
	struct v4l2_requestbuffers req = {
		.count = 4, .type = type, .memory = V4L2_MEMORY_MMAP,
	};
	struct {
		void *addr;
		size_t length;
	} *buffers = NULL;
	struct timeval previous = { 0 };
	uint32_t sequence = 0;
	unsigned int i, mapped = 0, received = 0, gaps = 0;
	unsigned long frames;
	double total_ms = 0;
	int fd = -1, streaming = 0, status = EXIT_FAILURE;
	FILE *output = NULL;
	char *end;

	if (argc != 4) {
		fprintf(stderr, "Usage: %s <video device> <frames> <output RAW>\n",
			argv[0]);
		return EXIT_FAILURE;
	}

	errno = 0;
	frames = strtoul(argv[2], &end, 10);
	if (errno || argv[2][0] < '0' || argv[2][0] > '9' || *end ||
	    !frames || frames > UINT_MAX) {
		fprintf(stderr, "Invalid frame count: %s\n", argv[2]);
		return EXIT_FAILURE;
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		perror(argv[1]);
		goto out;
	}

	if (video_ioctl(fd, VIDIOC_G_FMT, &fmt, "VIDIOC_G_FMT") < 0)
		goto out;

	printf("width=%u height=%u bytesperline=%u sizeimage=%u\n",
	       fmt.fmt.pix.width, fmt.fmt.pix.height,
	       fmt.fmt.pix.bytesperline, fmt.fmt.pix.sizeimage);

	if (video_ioctl(fd, VIDIOC_REQBUFS, &req, "VIDIOC_REQBUFS") < 0)
		goto out;
	if (!req.count) {
		fprintf(stderr, "No capture buffers available\n");
		goto out;
	}

	buffers = calloc(req.count, sizeof(*buffers));
	if (!buffers) {
		perror("calloc");
		goto out;
	}

	for (i = 0; i < req.count; i++) {
		struct v4l2_buffer buf = {
			.type = type, .memory = req.memory, .index = i,
		};

		if (video_ioctl(fd, VIDIOC_QUERYBUF, &buf, "VIDIOC_QUERYBUF") < 0)
			goto out;

		buffers[i].length = buf.length;
		buffers[i].addr = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
				       MAP_SHARED, fd, buf.m.offset);
		if (buffers[i].addr == MAP_FAILED) {
			perror("mmap");
			goto out;
		}
		mapped++;
	}

	for (i = 0; i < req.count; i++) {
		struct v4l2_buffer buf = {
			.type = type, .memory = req.memory, .index = i,
		};

		if (video_ioctl(fd, VIDIOC_QBUF, &buf, "VIDIOC_QBUF") < 0)
			goto out;
	}

	if (video_ioctl(fd, VIDIOC_STREAMON, &type, "VIDIOC_STREAMON") < 0)
		goto out;
	streaming = 1;

	while (received < frames) {
		struct v4l2_buffer buf = {
			.type = type, .memory = req.memory,
		};
		double interval = 0;

		if (video_ioctl(fd, VIDIOC_DQBUF, &buf, "VIDIOC_DQBUF") < 0)
			goto out;
		if (buf.index >= req.count ||
		    buf.bytesused > buffers[buf.index].length) {
			fprintf(stderr, "Invalid capture buffer\n");
			goto out;
		}

		if (received) {
			interval = (buf.timestamp.tv_sec - previous.tv_sec) * 1000.0 +
				   (buf.timestamp.tv_usec - previous.tv_usec) / 1000.0;
			total_ms += interval;
			if (buf.sequence != (uint32_t)(sequence + 1U))
				gaps++;
		}

		printf("sequence=%u timestamp=%lld.%06ld interval_ms=%.3f bytesused=%u\n",
		       buf.sequence, (long long)buf.timestamp.tv_sec,
		       (long)buf.timestamp.tv_usec, interval, buf.bytesused);
		previous = buf.timestamp;
		sequence = buf.sequence;
		received++;

		if (received == frames) {
			output = fopen(argv[3], "wb");
			if (!output) {
				perror(argv[3]);
				goto out;
			}

			if (fwrite(buffers[buf.index].addr, 1, buf.bytesused,
				   output) != buf.bytesused) {
				perror("fwrite");
				goto out;
			}
		}

		if (video_ioctl(fd, VIDIOC_QBUF, &buf, "VIDIOC_QBUF") < 0)
			goto out;
	}

	status = EXIT_SUCCESS;

out:
	if (streaming &&
	    video_ioctl(fd, VIDIOC_STREAMOFF, &type, "VIDIOC_STREAMOFF") < 0)
		status = EXIT_FAILURE;

	for (i = 0; i < mapped; i++) {
		if (munmap(buffers[i].addr, buffers[i].length) < 0) {
			perror("munmap");
			status = EXIT_FAILURE;
		}
	}
	free(buffers);

	if (fd >= 0 && close(fd) < 0) {
		perror("close video");
		status = EXIT_FAILURE;
	}

	if (output && fclose(output) != 0) {
		perror("fclose RAW");
		status = EXIT_FAILURE;
	}

	if (status == EXIT_SUCCESS)
		printf("frames=%u average_interval_ms=%.3f sequence_gaps=%u\n",
		       received, received > 1 ? total_ms / (received - 1) : 0, gaps);

	return status;
}
