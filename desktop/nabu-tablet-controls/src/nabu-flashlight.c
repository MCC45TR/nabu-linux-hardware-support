#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_PERCENT 13

static const char *const brightness_paths[] = {
	"/sys/class/leds/white:flash/brightness",
	"/sys/class/leds/yellow:flash/brightness",
};

static const char *const max_brightness_paths[] = {
	"/sys/class/leds/white:flash/max_brightness",
	"/sys/class/leds/yellow:flash/max_brightness",
};

static void force_off(const char *path)
{
	int fd = open(path, O_WRONLY | O_CLOEXEC | O_NOFOLLOW);

	if (fd >= 0) {
		ssize_t written = write(fd, "0\n", 2);

		if (written != 2)
			errno = EIO;
		close(fd);
	}
}

static int read_integer(const char *path, int *result)
{
	char value[32] = {0};
	char *end;
	long parsed;
	int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	ssize_t len;

	if (fd < 0)
		return -1;
	len = read(fd, value, sizeof(value) - 1);
	close(fd);
	if (len <= 0)
		return -1;
	errno = 0;
	parsed = strtol(value, &end, 10);
	if (errno || end == value || parsed < 0 || parsed > 1000000)
		return -1;
	*result = (int)parsed;
	return 0;
}

static int read_percent(void)
{
	int brightness, maximum, percent = -1;

	for (size_t i = 0; i < sizeof(brightness_paths) / sizeof(brightness_paths[0]); i++) {
		if (!read_integer(brightness_paths[i], &brightness) &&
		    !read_integer(max_brightness_paths[i], &maximum) && maximum > 0) {
			int current = brightness == 0 ? 0 :
				(brightness * 100 + maximum / 2) / maximum;

			if (current > percent)
				percent = current;
		}
	}
	return percent;
}

static int write_percent(int percent)
{
	int available = 0;
	int maximum;

	for (size_t i = 0; i < sizeof(brightness_paths) / sizeof(brightness_paths[0]); i++) {
		char value[32];
		int level, fd, length;
		ssize_t written;

		if (read_integer(max_brightness_paths[i], &maximum) < 0 || maximum <= 0)
			continue;
		available++;
		level = percent == 0 ? 0 : (maximum * percent + 99) / 100;
		if (level > maximum)
			level = maximum;
		length = snprintf(value, sizeof(value), "%d\n", level);
		fd = open(brightness_paths[i], O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
		if (fd < 0) {
			for (size_t j = 0; j < i; j++)
				force_off(brightness_paths[j]);
			return -1;
		}
		written = write(fd, value, (size_t)length);
		close(fd);
		if (written != length) {
			for (size_t j = 0; j <= i; j++)
				force_off(brightness_paths[j]);
			return -1;
		}
	}
	return available ? 0 : -1;
}

int main(int argc, char **argv)
{
	int percent, requested = DEFAULT_PERCENT;

	if (argc < 2 || argc > 3) {
		fprintf(stderr, "usage: nabu-flashlight status|on [percent]|off|toggle [percent]|set percent\n");
		return 2;
	}
	percent = read_percent();
	if (percent < 0) {
		errno = ENODEV;
		fprintf(stderr, "flash LED unavailable: %s\n", strerror(errno));
		return 1;
	}
	if (!strcmp(argv[1], "status")) {
		if (percent)
			printf("on %d\n", percent);
		else
			puts("off 0");
		return 0;
	}
	if (argc == 3) {
		char *end;
		long value;

		errno = 0;
		value = strtol(argv[2], &end, 10);
		if (errno || *end || value < 1 || value > 100) {
			fprintf(stderr, "percent must be between 1 and 100\n");
			return 2;
		}
		requested = (int)value;
	}
	if (!strcmp(argv[1], "on"))
		percent = requested;
	else if (!strcmp(argv[1], "off"))
		percent = 0;
	else if (!strcmp(argv[1], "toggle"))
		percent = percent ? 0 : requested;
	else if (!strcmp(argv[1], "set") && argc == 3)
		percent = requested;
	else {
		fprintf(stderr, "unknown command\n");
		return 2;
	}
	if (write_percent(percent) < 0) {
		fprintf(stderr, "cannot control flash LED: %s\n", strerror(errno));
		return 1;
	}
	if (percent)
		printf("on %d\n", percent);
	else
		puts("off 0");
	return 0;
}
