// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/select.h>
#include <sys/time.h>

#include <sys/timeb.h>

struct app_config {
	char *host;
	char *service;
};

int parse_argv(struct app_config *conf, int argc, char ** argv);
int connect_gai(char *host, char *service);
int connect_rfc6555(char *host, char *service);
int socket_connect(struct addrinfo *rp);
int rfc6555(struct addrinfo *result, int sfd);
struct addrinfo *find_ipv4_addrinfo(struct addrinfo *result);
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
	/* if((sfd = connect_gai(conf.host, conf.service)) < 0) */
	if((sfd = connect_rfc6555(conf.host, conf.service)) < 0)
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

/* Simple connection using getaddrinfo(3), from the manpage;
 * licensing terms for this function can be found at [0].
 *
 * [0] http://man7.org/linux/man-pages/man3/getaddrinfo.3.license.html
 */
int connect_gai(char *host, char *service) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags |= AI_CANONNAME;
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
		fprintf(stderr, "connecting using rp %p (%s, af %d) ...",
				rp,
				rp->ai_canonname,
				rp->ai_family);
		sfd = socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		if (sfd == -1)
			continue;

		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;                  /* Success */

		fprintf(stderr, " failed!\n");
		perror("error: connecting: ");
		close(sfd);
	}

	if (rp == NULL) {               /* No address succeeded */
		fprintf(stderr, "failed! (last attempt)\n");
		perror("error: connecting: ");
		return -3;
	}
	fprintf(stderr, " success!\n");

	freeaddrinfo(result);           /* No longer needed */

	return sfd;
}

/* Variation on the above, to implement RFC6555.
 *
 * Licensing terms for this function can be found at [0].
 *
 * [0] http://man7.org/linux/man-pages/man3/getaddrinfo.3.license.html
 */
int connect_rfc6555(char *host, char *service) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags |= AI_CANONNAME;
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
		sfd = socket_connect(rp);
		if (sfd == -1)
			continue;

		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;                  /* Success */

		if (EINPROGRESS == errno) {
			fprintf(stderr, " in progress ... \n");
			if((sfd = rfc6555(rp, sfd)) > -1)
				break;
				}

		perror("error: connecting: ");
		close(sfd);
	}

	if (rp == NULL) {               /* No address succeeded */
		fprintf(stderr, "failed! (last attempt)\n");
		perror("error: connecting: ");
		return -3;
	}
	fprintf(stderr, " success!\n");

	freeaddrinfo(result);           /* No longer needed */

	return sfd;
}

int socket_connect(struct addrinfo *rp) {
	int sfd;
	int flags;

	fprintf(stderr, "connecting using rp %p (%s, af %d) ...",
			rp,
			rp->ai_canonname,
			rp->ai_family);

	sfd = socket(rp->ai_family, rp->ai_socktype,
			rp->ai_protocol);
	if (sfd == -1)
		return -1;

	flags = fcntl(sfd, F_GETFL,0);
	fcntl(sfd, F_SETFL, flags | O_NONBLOCK);

	return sfd;
}

int rfc6555(struct addrinfo *result, int sfd) {
	fd_set readfds, writefds;
	int ret;
	struct addrinfo *rpv4;
	int sfdv4;

	struct timeval timeout = { 0, 300000 }; /* 300ms */

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_SET(sfd, &readfds);
	FD_SET(sfd, &writefds);

	fprintf(stderr, "info: waiting for 300ms ...\n");
	/* select with 300ms TO */
	if((ret = select(1, &readfds, &writefds, NULL, &timeout))) {
		perror("error: initial timeout");
		return -1;
	}

	if (ret == 1) {
		return sfd;
	}

	fprintf(stderr, "info: still in progress, findind IPv4 ...\n");
	/* find IPv4 address */
	if(NULL == (rpv4 = find_ipv4_addrinfo(result->ai_next))) {
		/* none found */
		return sfd;
	}
	if (-1 == (sfdv4 = socket_connect(rpv4))) {
		perror("error: setting up IPv4 socket");
		return sfd;
	}

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_SET(sfd, &readfds);
	FD_SET(sfdv4, &readfds);
	FD_SET(sfd, &writefds);
	FD_SET(sfdv4, &writefds);

	fprintf(stderr, "info: waiting for any socket ...\n");
	/* select with 300ms TO */
	if((ret = select(1, &readfds, &writefds, NULL, &timeout))) {
		perror("error: initial timeout");
		return -1;
	}

	if (ret >= 1) {
		if (FD_ISSET(sfd, &readfds) || FD_ISSET(sfd, &writefds)) {
			fprintf(stderr, "info: IPv6 selected\n");
			return sfd;
		}
		else if (FD_ISSET(sfdv4, &readfds) || FD_ISSET(sfdv4, &writefds)) {
			fprintf(stderr, "info: IPv4 selected\n");
			return sfdv4;
		}
	}
	return -1;
}

struct addrinfo *find_ipv4_addrinfo(struct addrinfo *result) {
	for (; result != NULL; result = result->ai_next) {
		if (AF_INET == result->ai_family) {
			return result;
		}
	}
	return NULL;
}

void print_delta(struct timeb *start, struct timeb *stop) {
	fprintf(stderr, "delta: %lds %dms\n",
			stop->time - start->time,
			stop->millitm-start->millitm);
}


int try_read(int sfd) {
	char buf[1];
	ssize_t s;

	if((s = read(sfd, buf, sizeof(buf))) < 0) {
		perror("error: reading: ");
		return -4;
	}

	fprintf(stderr, "read: ");
	printf("%s\n", buf);

	return 0;
}
