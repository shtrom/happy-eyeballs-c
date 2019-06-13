// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <sys/timeb.h>

struct app_config {
	char *host;
	char *service;
};

int parse_argv(struct app_config *conf, int argc, char ** argv);
int connect_host(char *host, char *service);
void print_delta(struct timeb *start, struct timeb *stop);
int try_read(int sfd);

int main(int argc, char **argv) {
	struct app_config conf;
	int ret = 0;
	int sfd = 0;
	struct timeb start_all, start, stop;

	if (0 != (ret = parse_argv(&conf, argc, argv))) {
		fprintf(stderr, "error: parsing arguments: %d\n", ret);
		fprintf(stderr, "usage: %s HOST PORT\n", argv[0]);
		return ret;
	}

	fprintf(stderr, "happy-eyeballing %s:%s ... \n", conf.host, conf.service);

	ftime(&start_all);
	ftime(&start);
	if((sfd = connect_host(conf.host, conf.service)) < 0)
	{
		fprintf(stderr, "error: connecting: %d\n", sfd);
		return sfd;
	}
	ftime(&stop);

	print_delta(&start, &stop);

	fprintf(stderr, "reading ...\n");

	ftime(&start);
	if ((ret = try_read(sfd)) < 0) {
		fprintf(stderr, "error: reading: %d\n", ret);
		return ret;
	}
	ftime(&stop);

	print_delta(&start, &stop);
	print_delta(&start_all, &stop);

	return ret;
}

int parse_argv(struct app_config *conf, int argc, char ** argv) {
	if (argc < 3) {
		return -1;
	}

	conf->host = strdup(argv[1]);
	conf->service = strdup(argv[2]);

	return 0;
}


void print_delta(struct timeb *start, struct timeb *stop) {
	fprintf(stderr, "delta: %lds %dms\n",
			stop->time - start->time,
			stop->millitm-start->millitm);
}


int try_read(int sfd) {
	char buf[1024];
	ssize_t s;

	while((s = read(sfd, buf, sizeof(buf))) < 0) {
		if (EAGAIN != errno) {
			perror("error: reading: ");
			return -4;
		}
	}

	fprintf(stderr, "read: ");
	printf("%s\n", buf);

	return 0;
}
