// usage example:
// gpio-detector /dev/gpiochip1 12 /var/rise_mark

#include <linux/gpio.h>
#include <sys/ioctl.h>
#include <sys/poll.h>

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#define NEXT_POLL_TIME 60 // seconds
#define CONSUMER_LABEL "gpio-detector"
#define TIME_LENGTH 128

typedef enum {
	ERR_ARG = 1,
	ERR_OPEN = 2,
	ERR_GET_LINEEVENT_IOCTL = 3,
	ERR_POLL = 4,
	ERR_WRITE = 5
} error_t;

void format_time(char *output) {
	time_t rawtime;
	struct tm * timeinfo;
	
	time(&rawtime);
	timeinfo = localtime(&rawtime);

	sprintf(output, "%d %d %d %d:%02d:%02d", timeinfo->tm_mday,
			timeinfo->tm_mon + 1, timeinfo->tm_year + 1900,
			timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}

int make_mark(const char *path)
{
	int fd;
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if(fd < 0) return ERR_OPEN;

	char str_time[TIME_LENGTH];
	format_time(str_time);

	size_t text_length = strlen(str_time);
	size_t written = write(fd, str_time, text_length);
	close(fd);

	if(written < text_length)
		return ERR_WRITE;

	return 0;
}

int gpio_poll(const char *dev, int offset) {
	struct gpioevent_request rq;
	struct pollfd pfd;
	int fd, ret;

	fd = open(dev, O_RDONLY);
	if(fd < 0) return ERR_OPEN;

	memset(&rq, 0, sizeof(struct gpioevent_request));
	rq.lineoffset = offset;
	rq.eventflags = GPIOEVENT_EVENT_RISING_EDGE;
	rq.handleflags = GPIOHANDLE_REQUEST_INPUT;
	strcpy(rq.consumer_label, CONSUMER_LABEL);

	ret = ioctl(fd, GPIO_GET_LINEEVENT_IOCTL, &rq);

	close(fd);
	if(ret < 0) return ERR_GET_LINEEVENT_IOCTL;

	pfd.fd = rq.fd;
	pfd.events = POLLIN | POLLPRI;

	ret = poll(&pfd, 1, -1);
	if(ret < 0) {
		close(rq.fd);
		return ERR_POLL;
	}

	close(rq.fd);
	return 0;
}

int main(int argc, char *argv[])
{
	int ret, input_offset = -1;

	if (argc != 4) {
		fprintf(stderr, "usage: %s <input-device> <input-offset> <output-file>\n",
			argv[0]);
		return ERR_ARG;
	}

	if (sscanf(argv[2], "%d", &input_offset) != 1 || input_offset < 0) {
		fprintf(stderr, "%s: invalid <input-offset> value.\n", argv[0]);
		return ERR_ARG;
	}
	
	do {
		ret = gpio_poll(argv[1], input_offset);
		if(ret != 0) {
			printf("gpio_poll error: %d\n", ret);
			return 1;
		}
		make_mark(argv[3]);
		sleep(NEXT_POLL_TIME);
	} while(1);

	return 0;
}
