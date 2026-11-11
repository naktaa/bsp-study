// SPDX-License-Identifier: GPL-2.0
#include <alsa/asoundlib.h>
#include <endian.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define SAMPLE_LIMIT 8388607

struct capture_options {
	const char *device;
	const char *output;
	unsigned int rate;
	snd_pcm_uframes_t period;
	snd_pcm_uframes_t buffer;
	unsigned int gain;
	unsigned int delay_ms;
	unsigned int duration;
};

struct pcm_config {
	unsigned int rate;
	snd_pcm_uframes_t period;
	snd_pcm_uframes_t buffer;
};

struct sample_stats {
	unsigned int frames;
	unsigned int peak;
	unsigned int clips;
	double squares;
};

static volatile sig_atomic_t stopped;

static void stop_capture(int signal_number)
{
	(void)signal_number;
	stopped = 1;
}

static void usage(const char *program)
{
	fprintf(stderr,
		"Usage: %s [-D device] [-r rate] [-p period] [-b buffer]\n"
		"       [-g gain] [-s delay_ms] [-d seconds] -o path|-\n",
		program);
}

static int parse_number(const char *text, unsigned long maximum,
			unsigned long *value)
{
	char *end;

	if (*text < '0' || *text > '9')
		return -EINVAL;

	errno = 0;
	*value = strtoul(text, &end, 10);
	if (errno || *end || *value > maximum)
		return -EINVAL;

	return 0;
}

static int parse_options(int argc, char **argv, struct capture_options *options)
{
	unsigned long value;
	unsigned long maximum;
	int option;

	*options = (struct capture_options) {
		.device = "hw:CARD=inmp441,DEV=0",
		.rate = 48000,
		.period = 1024,
		.buffer = 4096,
		.gain = 1,
		.duration = 5,
	};

	while ((option = getopt(argc, argv, "D:r:p:b:g:s:d:o:")) != -1) {
		if (option == 'D') {
			options->device = optarg;
			continue;
		}
		if (option == 'o') {
			options->output = optarg;
			continue;
		}
		if (option == '?')
			return -EINVAL;

		maximum = UINT_MAX;
		if (option == 'r')
			maximum = UINT32_MAX / 4;
		else if (option == 'p')
			maximum = SIZE_MAX / (2 * sizeof(int32_t));
		else if (option == 'b')
			maximum = LONG_MAX;
		else if (option == 'g')
			maximum = INT_MAX;

		if (parse_number(optarg, maximum, &value))
			return -EINVAL;
		if (!value && option != 'd' && option != 's')
			return -EINVAL;

		switch (option) {
		case 'r':
			options->rate = value;
			break;
		case 'p':
			options->period = value;
			break;
		case 'b':
			options->buffer = value;
			break;
		case 'g':
			options->gain = value;
			break;
		case 's':
			options->delay_ms = value;
			break;
		case 'd':
			options->duration = value;
			break;
		}
	}

	if (optind != argc || !*options->device || !options->output ||
	    !*options->output || options->buffer < options->period)
		return -EINVAL;

	return 0;
}

static int setup_capture(snd_pcm_t *pcm, const struct capture_options *options,
			 struct pcm_config *config)
{
	snd_pcm_hw_params_t *params;
	int err;

	snd_pcm_hw_params_alloca(&params);
	config->rate = options->rate;
	config->period = options->period;
	config->buffer = options->buffer;

	err = snd_pcm_hw_params_any(pcm, params);
	if (err < 0)
		return err;
	err = snd_pcm_hw_params_set_access(pcm, params,
					   SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0)
		return err;
	err = snd_pcm_hw_params_set_format(pcm, params, SND_PCM_FORMAT_S32_LE);
	if (err < 0)
		return err;
	err = snd_pcm_hw_params_set_channels(pcm, params, 2);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_set_rate_near(pcm, params, &config->rate, NULL);
	if (err < 0)
		return err;
	err = snd_pcm_hw_params_set_period_size_near(pcm, params,
						    &config->period, NULL);
	if (err < 0)
		return err;
	err = snd_pcm_hw_params_set_buffer_size_near(pcm, params,
						    &config->buffer);
	if (err < 0)
		return err;
	err = snd_pcm_hw_params(pcm, params);
	if (err < 0)
		return err;

	err = snd_pcm_hw_params_get_rate(params, &config->rate, NULL);
	if (err < 0)
		return err;
	err = snd_pcm_hw_params_get_period_size(params, &config->period, NULL);
	if (err < 0)
		return err;
	err = snd_pcm_hw_params_get_buffer_size(params, &config->buffer);
	if (err < 0)
		return err;
	if (!config->rate || config->rate > UINT32_MAX / 4 || !config->period ||
	    config->period > SIZE_MAX / (2 * sizeof(int32_t)))
		return -EINVAL;

	fprintf(stderr,
		"[PCM] requested rate=%u period=%lu buffer=%lu; "
		"actual rate=%u period=%lu (%.3f ms) buffer=%lu (%.3f ms)\n",
		options->rate, options->period, options->buffer,
		config->rate, config->period, 1000.0 * config->period / config->rate,
		config->buffer, 1000.0 * config->buffer / config->rate);
	return 0;
}

static void put_le32(unsigned char *destination, uint32_t value)
{
	value = htole32(value);
	memcpy(destination, &value, sizeof(value));
}

static int write_wav_header(FILE *output, unsigned int rate, uint32_t bytes)
{
	unsigned char header[44] = { 0 };

	memcpy(header, "RIFF", 4);
	put_le32(header + 4, 36 + bytes);
	memcpy(header + 8, "WAVEfmt ", 8);
	put_le32(header + 16, 16);
	header[20] = 1;
	header[22] = 1;
	put_le32(header + 24, rate);
	put_le32(header + 28, rate * 4);
	header[32] = 4;
	header[34] = 32;
	memcpy(header + 36, "data", 4);
	put_le32(header + 40, bytes);

	if (fwrite(header, 1, sizeof(header), output) != sizeof(header))
		return errno ? -errno : -EIO;

	return 0;
}

static void process_samples(const int32_t *input, uint32_t *output,
			    snd_pcm_uframes_t frames, unsigned int rate,
			    unsigned int gain, int wav,
			    struct sample_stats *stats, uint64_t xruns)
{
	snd_pcm_uframes_t frame;

	for (frame = 0; frame < frames; frame++) {
		int32_t sample = (int32_t)le32toh((uint32_t)input[frame * 2]) >> 8;
		unsigned int magnitude = sample < 0 ? -sample : sample;
		int64_t scaled = (int64_t)sample * gain;
		uint32_t encoded;

		if (magnitude > stats->peak)
			stats->peak = magnitude;
		stats->squares += (double)sample * sample;
		if (scaled > SAMPLE_LIMIT) {
			scaled = SAMPLE_LIMIT;
			stats->clips++;
		} else if (scaled < -SAMPLE_LIMIT) {
			scaled = -SAMPLE_LIMIT;
			stats->clips++;
		}

		/* 음수 좌시프트의 미정의 동작을 피하고 같은 비트 배열을 만든다. */
		encoded = (uint32_t)(int32_t)scaled;
		if (wav)
			encoded <<= 8;
		output[frame] = htole32(encoded);

		if (++stats->frames == rate) {
			fprintf(stderr,
				"[LEVEL] peak=%u rms=%.1f clip=%u xrun=%" PRIu64 "\n",
				stats->peak, sqrt(stats->squares / rate),
				stats->clips, xruns);
			*stats = (struct sample_stats) { 0 };
		}
	}
}

static int capture_loop(snd_pcm_t *pcm, FILE *output, int wav,
			const struct capture_options *options,
			const struct pcm_config *config, uint32_t *data_bytes)
{
	struct sample_stats stats = { 0 };
	struct timespec start, now, delay;
	int32_t *input;
	uint32_t *samples;
	uint64_t xruns = 0;
	int err = 0;

	input = malloc(config->period * 2 * sizeof(*input));
	samples = malloc(config->period * sizeof(*samples));
	if (!input || !samples) {
		err = -ENOMEM;
		goto out;
	}
	if (clock_gettime(CLOCK_MONOTONIC, &start) < 0) {
		err = -errno;
		goto out;
	}

	while (!stopped) {
		snd_pcm_sframes_t frames;
		size_t written;

		if (options->duration) {
			if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
				err = -errno;
				break;
			}
			if (now.tv_sec - start.tv_sec +
			    (now.tv_nsec - start.tv_nsec) / 1e9 >= options->duration)
				break;
		}

		frames = snd_pcm_readi(pcm, input, config->period);
		if (frames < 0) {
			if (stopped)
				break;
			if (frames == -EPIPE)
				fprintf(stderr, "[XRUN] %" PRIu64 "\n", ++xruns);
			err = snd_pcm_recover(pcm, frames, 1);
			if (err < 0)
				break;
			continue;
		}

		process_samples(input, samples, frames, config->rate,
				options->gain, wav, &stats, xruns);
		if (wav && (uint64_t)*data_bytes + frames * sizeof(*samples) >
		    UINT32_MAX - 36) {
			err = -EFBIG;
			break;
		}

		errno = 0;
		written = fwrite(samples, sizeof(*samples), frames, output);
		if (wav)
			*data_bytes += written * sizeof(*samples);
		if (written != (size_t)frames || (!wav && fflush(output) == EOF)) {
			err = errno ? -errno : -EIO;
			if (!wav && err == -EPIPE)
				err = 0;
			break;
		}

		delay.tv_sec = options->delay_ms / 1000;
		delay.tv_nsec = (options->delay_ms % 1000) * 1000000L;
		while (options->delay_ms && !stopped &&
		       nanosleep(&delay, &delay) < 0) {
			if (errno != EINTR) {
				err = -errno;
				goto out;
			}
		}
	}

out:
	free(samples);
	free(input);
	return err;
}

int main(int argc, char **argv)
{
	struct capture_options options;
	struct pcm_config config;
	struct sigaction action = { .sa_handler = stop_capture };
	snd_pcm_t *pcm = NULL;
	FILE *output = NULL;
	uint32_t data_bytes = 0;
	int header_written = 0;
	int status = EXIT_FAILURE;
	int wav;
	int err;

	if (parse_options(argc, argv, &options)) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	wav = strcmp(options.output, "-") != 0;

	sigemptyset(&action.sa_mask);
	if (sigaction(SIGINT, &action, NULL) < 0) {
		perror("sigaction SIGINT");
		return EXIT_FAILURE;
	}
	action.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &action, NULL) < 0) {
		perror("sigaction SIGPIPE");
		return EXIT_FAILURE;
	}

	err = snd_pcm_open(&pcm, options.device, SND_PCM_STREAM_CAPTURE, 0);
	if (err < 0) {
		fprintf(stderr, "PCM open: %s\n", snd_strerror(err));
		goto out;
	}
	err = setup_capture(pcm, &options, &config);
	if (err < 0) {
		fprintf(stderr, "PCM setup: %s\n", snd_strerror(err));
		goto out;
	}

	output = wav ? fopen(options.output, "wb") : stdout;
	if (!output) {
		perror(options.output);
		goto out;
	}
	if (wav) {
		err = write_wav_header(output, config.rate, 0);
		if (err < 0) {
			fprintf(stderr, "WAV header: %s\n", snd_strerror(err));
			goto out;
		}
		header_written = 1;
	}

	err = capture_loop(pcm, output, wav, &options, &config, &data_bytes);
	if (err < 0)
		fprintf(stderr, "Capture: %s\n", snd_strerror(err));
	else
		status = EXIT_SUCCESS;

out:
	if (header_written) {
		if (fseek(output, 0, SEEK_SET) != 0) {
			perror("WAV seek");
			status = EXIT_FAILURE;
		} else {
			err = write_wav_header(output, config.rate, data_bytes);
			if (err < 0) {
				fprintf(stderr, "WAV finalize: %s\n", snd_strerror(err));
				status = EXIT_FAILURE;
			}
		}
	}
	if (output && fclose(output) == EOF && (wav || errno != EPIPE)) {
		perror("Output close");
		status = EXIT_FAILURE;
	}
	if (pcm) {
		err = snd_pcm_close(pcm);
		if (err < 0) {
			fprintf(stderr, "PCM close: %s\n", snd_strerror(err));
			status = EXIT_FAILURE;
		}
	}
	return status;
}
