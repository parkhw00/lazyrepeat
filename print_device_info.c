
#include <stdio.h>
#include <fcntl.h>
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
	fprintf(stderr, fmt, ##args)

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

void print_device_info(char *dev_path)
{
	int fd_ui = open(dev_path, O_RDONLY);
	if (fd_ui < 0)
		fatal("open(%s) failed.\n", dev_path);

	char str[128];
	if (ioctl(fd_ui, EVIOCGNAME(sizeof(str)), str) >= 0)
	{
		str[sizeof(str)-1] = 0;
		info("%s - device name : %s\n", dev_path, str);
	}

	if (ioctl(fd_ui, EVIOCGPHYS(sizeof(str)), str) >= 0 )
	{
		str[sizeof(str)-1] = 0;
		info("%s - physical location : %s\n", dev_path, str);
	}

	//if (ioctl(fd_ui, EVIOCGUNIQ(sizeof(str)), str) >= 0)
	//{
	//	str[sizeof(str)-1] = 0;
	//	info("%s - unique identifier : %s\n", dev_path, str);
	//}

	//if (ioctl(fd_ui, EVIOCGPROP(sizeof(str)), str) >= 0)
	//{
	//	str[sizeof(str)-1] = 0;
	//	info("%s - device properties : %s\n", dev_path, str);
	//}

	close(fd_ui);
}

int main(int argc, char **argv)
{
	struct input_event event;

	int i;
	for (i=1; argv[i]; i++)
		print_device_info(argv[i]);

	return 0;
}

