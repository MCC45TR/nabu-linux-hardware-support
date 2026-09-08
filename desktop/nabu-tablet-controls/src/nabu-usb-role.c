#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef SYSTEMCTL_PATH
#define SYSTEMCTL_PATH "/usr/bin/systemctl"
#endif
#ifndef NABU_EFFECTIVE_UID
#define NABU_EFFECTIVE_UID() geteuid()
#endif

static const char *const data_role = "/sys/class/typec/port0/data_role";
static const char *const power_role = "/sys/class/typec/port0/power_role";
static const char *const port_type = "/sys/class/typec/port0/port_type";
static const char *const gadget_unit = "nabu-usb-gadget.service";

static int read_value(const char *path, char *value, size_t size)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	ssize_t len;

	if (fd < 0)
		return -1;
	len = read(fd, value, size - 1);
	close(fd);
	if (len <= 0)
		return -1;
	value[len] = '\0';
	return 0;
}

static bool current_is(const char *value, const char *role)
{
	char selected[32];

	if (snprintf(selected, sizeof(selected), "[%s]", role) < 0)
		return false;
	return strstr(value, selected) != NULL;
}

static int write_role(const char *path, const char *role)
{
	char state[96];
	struct stat st;
	int fd;
	ssize_t length = (ssize_t)strlen(role);

	if (NABU_EFFECTIVE_UID() != 0) {
		fprintf(stderr, "USB role changes require polkit authorization\n");
		return 1;
	}
	if (path != port_type &&
	    (read_value(port_type, state, sizeof(state)) < 0 ||
	     strstr(state, "dual") == NULL)) {
		fprintf(stderr, "USB-C port is not in dual-role mode\n");
		return 1;
	}
	fd = open(path, O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
	if (fd < 0 || fstat(fd, &st) < 0 || !S_ISREG(st.st_mode) || st.st_uid != 0) {
		if (fd >= 0)
			close(fd);
		fprintf(stderr, "cannot open the kernel role switch: %s\n", strerror(errno));
		return 1;
	}
	if (write(fd, role, (size_t)length) != length) {
		close(fd);
		fprintf(stderr, "kernel rejected the requested USB role: %s\n", strerror(errno));
		return 1;
	}
	close(fd);
	for (int retry = 0; retry < 20; retry++) {
		usleep(100000);
		if (!read_value(path, state, sizeof(state)) && current_is(state, role)) {
			printf("%s", state);
			return 0;
		}
	}
	fprintf(stderr, "USB partner did not accept the requested role\n");
	return 1;
}

static int set_power_policy(const char *role)
{
	if (!strcmp(role, "auto"))
		return write_role(port_type, "dual");
	if (!strcmp(role, "source") || !strcmp(role, "sink"))
		return write_role(power_role, role);
	fprintf(stderr, "unsupported USB power policy\n");
	return 2;
}

static int gadget_unit_load_state(char *value, size_t size)
{
	int pipefd[2];
	int status;
	pid_t child;
	ssize_t len;

	if (size < 2 || pipe(pipefd) < 0)
		return -1;
	child = fork();
	if (child < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}
	if (child == 0) {
		close(pipefd[0]);
		if (dup2(pipefd[1], STDOUT_FILENO) < 0)
			_exit(127);
		close(pipefd[1]);
		execl(SYSTEMCTL_PATH, "systemctl", "show",
		      "--property=LoadState", "--value", "--",
		      gadget_unit, (char *)NULL);
		_exit(127);
	}
	close(pipefd[1]);
	len = read(pipefd[0], value, size - 1);
	close(pipefd[0]);
	if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != 0 || len <= 0)
		return -1;
	while (len > 0 && (value[len - 1] == '\n' || value[len - 1] == '\r'))
		len--;
	value[len] = '\0';
	return 0;
}

static int gadget_service(const char *action)
{
	char load_state[32];
	pid_t child;
	int status;

	if (NABU_EFFECTIVE_UID() != 0) {
		fprintf(stderr, "USB mode changes require polkit authorization\n");
		return 1;
	}
	/* A host-only installation has nothing to tear down.  Keep every other
	 * systemctl failure fatal so an active gadget is never ignored. */
	if (!strcmp(action, "stop") &&
	    gadget_unit_load_state(load_state, sizeof(load_state)) == 0 &&
	    !strcmp(load_state, "not-found"))
		return 0;
	child = fork();
	if (child < 0) {
		fprintf(stderr, "cannot start the USB mode manager: %s\n", strerror(errno));
		return 1;
	}
	if (child == 0) {
		execl(SYSTEMCTL_PATH, "systemctl", action, gadget_unit, (char *)NULL);
		_exit(127);
	}
	if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != 0) {
		fprintf(stderr, "USB gadget service rejected the requested mode\n");
		return 1;
	}
	return 0;
}

static int set_mode(const char *mode)
{
	if (!strcmp(mode, "host")) {
		if (gadget_service("stop"))
			return 1;
		return write_role(data_role, "host");
	}
	if (!strcmp(mode, "gadget") || !strcmp(mode, "device")) {
		if (write_role(data_role, "device"))
			return 1;
		return gadget_service("start");
	}
	if (!strcmp(mode, "off"))
		return gadget_service("stop");
	fprintf(stderr, "unsupported USB mode\n");
	return 2;
}

int main(int argc, char **argv)
{
	char data[96], power[96], type[96];

	if (argc == 2 && !strcmp(argv[1], "status")) {
		if (read_value(data_role, data, sizeof(data)) ||
		    read_value(power_role, power, sizeof(power)) ||
		    read_value(port_type, type, sizeof(type))) {
			fprintf(stderr, "USB-C role switch unavailable\n");
			return 1;
		}
		printf("data=%s", data);
		printf("power=%s", power);
		printf("type=%s", type);
		return 0;
	}
	if (argc != 4 || strcmp(argv[1], "set")) {
		fprintf(stderr, "usage: nabu-usb-role status|set mode host|gadget|off|set data host|device|set power auto|source|sink\n");
		return 2;
	}
	if (!strcmp(argv[2], "mode"))
		return set_mode(argv[3]);
	if (!strcmp(argv[2], "data") &&
	    (!strcmp(argv[3], "host") || !strcmp(argv[3], "device")))
		return write_role(data_role, argv[3]);
	if (!strcmp(argv[2], "power"))
		return set_power_policy(argv[3]);
	fprintf(stderr, "unsupported USB role\n");
	return 2;
}
