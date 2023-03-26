
#define _GNU_SOURCE
#include <poll.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/input.h>

#define fatal(fmt, args...) \
do { \
	fprintf(stderr, "%s.%d: %s\n", __func__, __LINE__, strerror(errno)); \
	fprintf(stderr, "%s.%d: "fmt, __func__, __LINE__, ##args); \
	exit(1); \
} while(0)

#define debug(fmt, args...)		\
	fprintf(stderr, "%s.%d: "fmt, __func__, __LINE__, ##args)

#define info(fmt, args...)		\
	fprintf(stdout, fmt, ##args)

#define rioctl(fd, cmd, arg)	_rioctl(__func__, __LINE__, fd, #cmd, cmd, arg)
int _rioctl(const char *func, int line, int fd, const char *cmd_name, unsigned long cmd, void *arg)
{
	int r = ioctl(fd, cmd, arg);
	if (r < 0)
	{
		fprintf(stderr, "%s.%d: ioctl(%s) failed. %s\n", func, line, cmd_name, strerror(errno));
		exit(1);
	}
	return r;
}

int device_count;
char **devices_path;
struct pollfd *devices_fds;

long get_usec(struct timeval *tv)
{
	static int cut;
	static long start_sec;
	static long start_usec;

	if (!cut)
	{
		start_sec  = tv->tv_sec;
		start_usec = tv->tv_usec;
		cut = 1;
	}

	long sec  = tv->tv_sec -  start_sec;
	long usec = tv->tv_usec - start_usec;

	return sec * 1000000 + usec;
}

char *_asprintf(char *fmt, ...)
{
	int r;
	va_list ap;
	char *str = NULL;

	va_start(ap, fmt);
	r = vasprintf(&str, fmt, ap);
	va_end(ap);

	if (r < 0)
		fatal("vasprintf() failed.\n");

	return str;
}

struct enums
{
	char *str;
	int val;
};

char *enum_str(struct enums *enums, int val)
{
	while (enums->str)
	{
		if (enums->val == val)
			return enums->str;
		enums ++;
	}

	return "Unknown";
}

struct enums enum_ev_type[] =
{
#define def_enum_ev_type(n)	{ #n, EV_##n, }
	def_enum_ev_type(SYN),
	def_enum_ev_type(KEY),
	def_enum_ev_type(REL),
	def_enum_ev_type(ABS),
	def_enum_ev_type(MSC),
	def_enum_ev_type(SW),
	def_enum_ev_type(LED),
	def_enum_ev_type(SND),
	def_enum_ev_type(REP),
	def_enum_ev_type(FF),
	def_enum_ev_type(PWR),
#undef  def_enum_ev_type
	{ },
};

struct enums enum_ev_syn[] =
{
#define def_enum_ev_syn(n)	{ #n, SYN_##n, }
	def_enum_ev_syn(REPORT),
	def_enum_ev_syn(CONFIG),
	def_enum_ev_syn(MT_REPORT),
	def_enum_ev_syn(DROPPED),
#undef  def_enum_ev_syn
	{ },
};

struct enums enum_ev_rel[] =
{
#define def_enum_ev_rel(n)	{ #n, REL_##n, }
	def_enum_ev_rel(X),
	def_enum_ev_rel(Y),
	def_enum_ev_rel(Z),
	def_enum_ev_rel(RX),
	def_enum_ev_rel(RY),
	def_enum_ev_rel(RZ),
	def_enum_ev_rel(HWHEEL),
	def_enum_ev_rel(DIAL),
	def_enum_ev_rel(WHEEL),
	def_enum_ev_rel(MISC),
#undef  def_enum_ev_rel
	{ },
};

struct enums enum_ev_abs[] =
{
#define def_enum_ev_abs(n)	{ #n, ABS_##n, }
	def_enum_ev_abs(X),
	def_enum_ev_abs(Y),
	def_enum_ev_abs(Z),
	def_enum_ev_abs(RX),
	def_enum_ev_abs(RY),
	def_enum_ev_abs(RZ),
	def_enum_ev_abs(THROTTLE),
	def_enum_ev_abs(RUDDER),
	def_enum_ev_abs(WHEEL),
	def_enum_ev_abs(GAS),
	def_enum_ev_abs(BRAKE),
	def_enum_ev_abs(HAT0X),
	def_enum_ev_abs(HAT0Y),
	def_enum_ev_abs(HAT1X),
	def_enum_ev_abs(HAT1Y),
	def_enum_ev_abs(HAT2X),
	def_enum_ev_abs(HAT2Y),
	def_enum_ev_abs(HAT3X),
	def_enum_ev_abs(HAT3Y),
	def_enum_ev_abs(PRESSURE),
	def_enum_ev_abs(DISTANCE),
	def_enum_ev_abs(TILT_X),
	def_enum_ev_abs(TILT_Y),
	def_enum_ev_abs(TOOL_WIDTH),
	def_enum_ev_abs(VOLUME),
	def_enum_ev_abs(PROFILE),
	def_enum_ev_abs(MISC),
	def_enum_ev_abs(MT_SLOT),
	def_enum_ev_abs(MT_TOUCH_MAJOR),
	def_enum_ev_abs(MT_TOUCH_MINOR),
	def_enum_ev_abs(MT_WIDTH_MAJOR),
	def_enum_ev_abs(MT_WIDTH_MINOR),
	def_enum_ev_abs(MT_ORIENTATION),
	def_enum_ev_abs(MT_POSITION_X),
	def_enum_ev_abs(MT_POSITION_Y),
	def_enum_ev_abs(MT_TOOL_TYPE),
	def_enum_ev_abs(MT_BLOB_ID),
	def_enum_ev_abs(MT_TRACKING_ID),
	def_enum_ev_abs(MT_PRESSURE),
	def_enum_ev_abs(MT_DISTANCE),
	def_enum_ev_abs(MT_TOOL_X),
	def_enum_ev_abs(MT_TOOL_Y),
#undef  def_enum_ev_abs
	{ },
};

struct enums enum_ev_msc[] =
{
#define def_enum_ev_msc(n)	{ #n, MSC_##n, }
	def_enum_ev_msc(SERIAL),
	def_enum_ev_msc(PULSELED),
	def_enum_ev_msc(GESTURE),
	def_enum_ev_msc(RAW),
	def_enum_ev_msc(SCAN),
	def_enum_ev_msc(TIMESTAMP),
#undef  def_enum_ev_msc
	{ },
};

char *event_astr(struct input_event *ev)
{
	char *str = NULL;
	struct enums *code_enums = NULL;

	switch (ev->type)
	{
	case EV_SYN: code_enums = enum_ev_syn; break;
	case EV_REL: code_enums = enum_ev_rel; break;
	case EV_ABS: code_enums = enum_ev_abs; break;
	case EV_MSC: code_enums = enum_ev_msc; break;
	}

	str = _asprintf("%s %s",
			enum_str(enum_ev_type, ev->type),
			code_enums?enum_str(code_enums, ev->code):""
		       );

	return str;
}

int check_input(void)
{
	struct input_event event;

	int r;
	r = poll(devices_fds, device_count, -1);
	if (r < 0)
		fatal("poll() failed.\n");

	int i;
	for (i=0; i<device_count; i++)
	{
		if (devices_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
			fatal("poll failed for %s. revents 0x%x\n", devices_path[i], devices_fds[i].revents);
		if (devices_fds[i].revents & POLLIN)
		{
			ssize_t got = read(devices_fds[i].fd, &event, sizeof(event));
			if (got != sizeof(event))
				fatal("read() failed. got %s, expected %d, device %s\n",
						got, sizeof(event), devices_path[i]);

			long usec = get_usec(&event.time);

			char *str_time;
			if (1)
			{
				static long last;
				long diff = usec - last;
				last = usec;

				str_time = _asprintf("+%ld.%06ld", diff / 1000000, diff % 1000000);
			}
			else
				str_time = _asprintf("%ld.%06ld", usec / 1000000, usec % 1000000);

			char *desc = event_astr(&event);

			info("%s: %12s, type 0x%04x, code 0x%04x, value 0x%08x, # %s\n", devices_path[i],
					str_time,
					event.type, event.code, event.value,
					desc?desc:"");
			if (desc)
				free(desc);
			free(str_time);
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	int i;
	for (i=1; argv[i]; i++)
	{
		char *dev_path = argv[i];
		int dev_fd = open(dev_path, O_RDONLY);
		if (dev_fd < 0)
			fatal("open(%s) failed.\n", dev_path);

		devices_path = realloc(devices_path, sizeof(devices_path[0]) * (device_count+1));
		devices_path[device_count] = strdup(dev_path);

		devices_fds = realloc(devices_fds, sizeof(devices_fds[0]) * (device_count+1));
		devices_fds[device_count].fd = dev_fd;
		devices_fds[device_count].events = POLLIN;

		device_count ++;
	}

	while(check_input() >= 0)
		;

	return 0;
}

