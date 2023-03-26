
#define _GNU_SOURCE
#include <poll.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <linux/input.h>

#define fatal(fmt, args...) \
do { \
	fprintf(stderr, "%s.%d: %s\n", __func__, __LINE__, strerror(errno)); \
	fprintf(stderr, "%s.%d: "fmt, __func__, __LINE__, ##args); \
	exit(1); \
} while(0)

#if 0
#define debug(fmt, args...)		\
	fprintf(stderr, "%s.%d: "fmt, __func__, __LINE__, ##args)
#else
#define debug(fmt, args...)		\
	do{}while(0)
#endif

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

char *line_buf = NULL;
size_t line_len = 0;

struct event_info
{
	char *dev_path;
	struct timeval time;

	int type;
	int code;
	int value;
};

int parse_line(struct event_info *info, char *line)
{
	char *e = strchr(line, ':');
	if (!e)
		fatal("no devpath\n");
	info->dev_path = strndup(line, e-line);

	int time_sec = 0;
	int time_usec = 0;
	int r = sscanf(e+1, " +%d.%d, type %x, code %x, value %x,",
			&time_sec, &time_usec,
			&info->type, &info->code, &info->value);
	if (r != 5)
		fatal("sscanf() failed. %d, \"%s\"\n", r, e+1);

	info->time.tv_sec = time_sec;
	info->time.tv_usec = time_usec;

	return 0;
}

struct timeval tv_last;

struct input_dev
{
	char *path;
	int fd;
};

int dev_num;
struct input_dev *devs;

int check_input(void)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	if (tv_last.tv_sec == 0)
		tv_last = now;

	ssize_t r;
	r = getline(&line_buf, &line_len, stdin);
	if (r < 0)
		return -1;
	if (r > 0 && line_buf[r-1] == '\n')
		line_buf[r-1] = 0;

	struct event_info einfo = { };
	parse_line(&einfo, line_buf);

	int i;
	for (i=0; i<dev_num; i++)
	{
		if (!strcmp(devs[i].path, einfo.dev_path))
			break;
	}
	if (i == dev_num)
	{
		devs = realloc(devs, sizeof(devs[0]) * (i+1));
		devs[i].path = strdup(einfo.dev_path);
		devs[i].fd = open(einfo.dev_path, O_WRONLY | O_NDELAY);
		if (devs[i].fd < 0)
			fatal("open(%s) failed.\n", einfo.dev_path);

		dev_num ++;
	}

	free(einfo.dev_path);

	struct input_event event = { };
	event.type = einfo.type;
	event.code = einfo.code;
	event.value = einfo.value;
	timeradd(&tv_last, &einfo.time, &event.time);

	debug("%d.%06d: event.time %d.%06d, line : %s\n",
			now.tv_sec, now.tv_usec,
			event.time.tv_sec, event.time.tv_usec,
			line_buf);

	tv_last = event.time;

	if (timercmp(&event.time, &now, >))
	{
		struct timeval tsleep = { };
		timersub(&event.time, &now, &tsleep);
		debug("sleep %d.%06d\n", tsleep.tv_sec, tsleep.tv_usec);
		select(0, NULL, NULL, NULL, &tsleep);
	}

	if (write(devs[i].fd, &event, sizeof(event)) != sizeof(event))
		fatal("write() failed.\n");

	return 0;
}

int main(int argc, char **argv)
{
	while(check_input() >= 0)
		;

	return 0;
}

