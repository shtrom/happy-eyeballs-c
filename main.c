// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
int connect_gai(char *host, char *service);
void print_delta(struct timeb *start, struct timeb *stop);
int try_read(int sfd);

int main(int argc, char **argv) {
	struct app_config conf;
	int ret = 0;
	int sfd = 0;
	struct timeb start, stop;

	if (0 != (ret = parse_argv(&conf, argc, argv))) {
		fprintf(stderr, "error: parsing arguments: %d\n", ret);
		fprintf(stderr, "usage: %s HOST PORT\n", argv[0]);
		return ret;
	}

	fprintf(stderr, "happy-eyeballing %s:%s ... \n", conf.host, conf.service);

	ftime(&start);
	if((sfd = connect_gai(conf.host, conf.service)) < 0)
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

/* Simple connection using getaddrinfo(3), from the manpage */
int connect_gai(char *host, char *service) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_flags = 0;
	hints.ai_protocol = 0;          /* Any protocol */

	s = getaddrinfo(host,service, &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	/* getaddrinfo() returns a list of address structures.
	   Try each address until we successfully connect(2).
	   If socket(2) (or connect(2)) fails, we (close the socket
	   and) try the next address. */

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		fprintf(stderr, "connecting using rp %p (af %d) ...", rp, rp->ai_family);
		sfd = socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		if (sfd == -1)
			continue;

		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;                  /* Success */

		fprintf(stderr, " failed!\n");
		close(sfd);
	}

	if (rp == NULL) {               /* No address succeeded */
		fprintf(stderr, "failed! (last attempt)\n");
		return -3;
	}
	fprintf(stderr, " success!\n");

	freeaddrinfo(result);           /* No longer needed */

	return sfd;
}

void print_delta(struct timeb *start, struct timeb *stop) {
	fprintf(stderr, "delta: %lds %dms\n",
			stop->time - start->time,
			stop->millitm-start->millitm);
}


int try_read(int sfd) {
	char buf[1024];
	ssize_t s;

	if((s = read(sfd, buf, sizeof(buf))) < 0) {
		return -4;
	}

	fprintf(stderr, "read: ");
	printf("%s\n", buf);

	return 0;
}
