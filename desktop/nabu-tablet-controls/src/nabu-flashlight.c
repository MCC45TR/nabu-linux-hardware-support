#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define DEFAULT_PERCENT 13
#define MAX_V4L2_FLASH_DEVICES 8

static const char *const brightness_paths[] = {
	"/sys/class/leds/white:flash/brightness",
	"/sys/class/leds/yellow:flash/brightness",
};

static const char *const max_brightness_paths[] = {
	"/sys/class/leds/white:flash/max_brightness",
	"/sys/class/leds/yellow:flash/max_brightness",
};

static const char *const flash_names[] = {
	"white:flash",
	"yellow:flash",
};

struct v4l2_flash_device {
	char path[PATH_MAX];
	int fd;
	struct v4l2_queryctrl intensity;
};

static int ioctl_retry(int fd, unsigned long request, void *argument)
{
	int ret;

	do {
		ret = ioctl(fd, request, argument);
	} while (ret < 0 && errno == EINTR);
	return ret;
}

static int read_text(const char *path, char *value, size_t size)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	ssize_t len;

	if (fd < 0)
		return -1;
	len = read(fd, value, size - 1);
	close(fd);
	if (len <= 0)
		return -1;
	while (len > 0 && (value[len - 1] == '\n' || value[len - 1] == '\r'))
		len--;
	value[len] = '\0';
	return 0;
}

static int is_flash_name(const char *name)
{
	for (size_t i = 0; i < sizeof(flash_names) / sizeof(flash_names[0]); i++)
		if (!strcmp(name, flash_names[i]))
			return 1;
	return 0;
}

static void close_v4l2_devices(struct v4l2_flash_device *devices, size_t count)
{
	for (size_t i = 0; i < count; i++)
		if (devices[i].fd >= 0)
			close(devices[i].fd);
}

static int open_v4l2_devices(struct v4l2_flash_device *devices, size_t *count)
{
	glob_t matches = {0};
	int saved_errno = ENODEV;
	int ret;

	*count = 0;
	ret = glob("/sys/class/video4linux/v4l-subdev*", GLOB_NOSORT, NULL, &matches);
	if (ret == GLOB_NOMATCH) {
		errno = ENODEV;
		return -1;
	}
	if (ret) {
		errno = EIO;
		return -1;
	}

	for (size_t i = 0; i < matches.gl_pathc; i++) {
		char name_path[PATH_MAX];
		char name[64];
		const char *base;
		struct v4l2_queryctrl query = {
			.id = V4L2_CID_FLASH_TORCH_INTENSITY,
		};
		int length;

		length = snprintf(name_path, sizeof(name_path), "%s/name", matches.gl_pathv[i]);
		if (length < 0 || (size_t)length >= sizeof(name_path) ||
		    read_text(name_path, name, sizeof(name)) < 0 || !is_flash_name(name))
			continue;
		if (*count >= MAX_V4L2_FLASH_DEVICES) {
			saved_errno = E2BIG;
			goto fail;
		}
		base = strrchr(matches.gl_pathv[i], '/');
		if (!base || !base[1])
			continue;
		length = snprintf(devices[*count].path, sizeof(devices[*count].path),
				  "/dev/%s", base + 1);
		if (length < 0 || (size_t)length >= sizeof(devices[*count].path)) {
			saved_errno = ENAMETOOLONG;
			goto fail;
		}
		devices[*count].fd = open(devices[*count].path,
					  O_RDWR | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
		if (devices[*count].fd < 0) {
			saved_errno = errno;
			goto fail;
		}
		if (ioctl_retry(devices[*count].fd, VIDIOC_QUERYCTRL, &query) < 0) {
			saved_errno = errno;
			close(devices[*count].fd);
			devices[*count].fd = -1;
			goto fail;
		}
		if (query.flags & V4L2_CTRL_FLAG_DISABLED || query.minimum <= 0 ||
		    query.maximum < query.minimum || query.step <= 0) {
			saved_errno = ENOTSUP;
			close(devices[*count].fd);
			devices[*count].fd = -1;
			goto fail;
		}
		devices[*count].intensity = query;
		(*count)++;
	}
	globfree(&matches);
	if (!*count) {
		errno = ENODEV;
		return -1;
	}
	return 0;

fail:
	globfree(&matches);
	close_v4l2_devices(devices, *count);
	errno = saved_errno;
	return -1;
}

static int get_v4l2_control(int fd, uint32_t id, int *value)
{
	struct v4l2_control control = { .id = id };

	if (ioctl_retry(fd, VIDIOC_G_CTRL, &control) < 0)
		return -1;
	*value = control.value;
	return 0;
}

static int set_v4l2_control(int fd, uint32_t id, int value)
{
	struct v4l2_control control = {
		.id = id,
		.value = value,
	};

	return ioctl_retry(fd, VIDIOC_S_CTRL, &control);
}

static int write_percent_v4l2(int percent)
{
	struct v4l2_flash_device devices[MAX_V4L2_FLASH_DEVICES] = {0};
	size_t count;
	int saved_errno = 0;

	if (open_v4l2_devices(devices, &count) < 0)
		return -1;

	/* Do not steal a flash device that a camera has armed for a capture. */
	for (size_t i = 0; i < count; i++) {
		int mode;

		if (get_v4l2_control(devices[i].fd, V4L2_CID_FLASH_LED_MODE, &mode) < 0) {
			saved_errno = errno;
			goto out;
		}
		if (mode == V4L2_FLASH_LED_MODE_FLASH) {
			saved_errno = EBUSY;
			goto out;
		}
	}

	for (size_t i = 0; i < count; i++) {
		if (!percent) {
			if (set_v4l2_control(devices[i].fd, V4L2_CID_FLASH_LED_MODE,
					     V4L2_FLASH_LED_MODE_NONE) < 0) {
				saved_errno = errno;
				goto rollback;
			}
			continue;
		}

		struct v4l2_queryctrl *query = &devices[i].intensity;
		int64_t target = ((int64_t)query->maximum * percent + 99) / 100;
		int intensity;

		if (target <= query->minimum)
			intensity = query->minimum;
		else
			intensity = query->minimum +
				(int)(((target - query->minimum + query->step / 2) /
				       query->step) * query->step);
		if (intensity > query->maximum)
			intensity = query->maximum;
		if (set_v4l2_control(devices[i].fd, V4L2_CID_FLASH_TORCH_INTENSITY,
				     intensity) < 0 ||
		    set_v4l2_control(devices[i].fd, V4L2_CID_FLASH_LED_MODE,
				     V4L2_FLASH_LED_MODE_TORCH) < 0) {
			saved_errno = errno;
			goto rollback;
		}
	}

	close_v4l2_devices(devices, count);
	return 0;

rollback:
	for (size_t i = 0; i < count; i++)
		set_v4l2_control(devices[i].fd, V4L2_CID_FLASH_LED_MODE,
				 V4L2_FLASH_LED_MODE_NONE);
out:
	close_v4l2_devices(devices, count);
	errno = saved_errno;
	return -1;
}

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
		int sysfs_errno = errno;

		if (write_percent_v4l2(percent) < 0) {
			int v4l2_errno = errno;

			errno = v4l2_errno == ENODEV ? sysfs_errno : v4l2_errno;
			fprintf(stderr, "cannot control flash LED: %s\n", strerror(errno));
			return 1;
		}
	}
	if (percent)
		printf("on %d\n", percent);
	else
		puts("off 0");
	return 0;
}
