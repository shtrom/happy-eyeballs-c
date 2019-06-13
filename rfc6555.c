/* Variation on the gai.c, to implement RFC6555.
 *
 * Licensing terms for this function can be found at [0].
 *
 * [0] http://man7.org/linux/man-pages/man3/getaddrinfo.3.license.html
 */
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

#define MAX(a,b) (((a)>(b))?(a):(b))

int socket_create(struct addrinfo *rp);
int rfc6555(struct addrinfo *result, int sfd);
struct addrinfo *find_ipv4_addrinfo(struct addrinfo *result);

int connect_host(char *host, char *service) {
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
		sfd = socket_create(rp);
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
	fprintf(stderr, " success: %d!\n", sfd);

	freeaddrinfo(result);           /* No longer needed */

	return sfd;
}

int socket_create(struct addrinfo *rp) {
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
	fd_set readfds, writefds, errorfds;
	int ret;
	struct addrinfo *rpv4;
	int sfdv4;

	struct timeval timeout = { 0, 300000 }; /* 300ms */

	FD_ZERO(&readfds);
	FD_SET(sfd, &readfds);
	writefds = readfds;
	errorfds = readfds;

	fprintf(stderr, "info: waiting for 300ms ...\n");
	/* select with 300ms TO */
	if((ret = select(sfd+1, &readfds, &writefds, &errorfds, &timeout)) < 0)  {
		perror("error: initial timeout");
		return -1;
	}

	if (ret == 1 && !FD_ISSET(sfd, &errorfds)) {
		return sfd;
	}

	fprintf(stderr, "info: still in progress, finding IPv4 ...\n");
	/* find IPv4 address */
	if(NULL == (rpv4 = find_ipv4_addrinfo(result->ai_next))) {
		fprintf(stderr, "error: none found, IPv6 selected\n");
		return sfd;
	}
	if (-1 == (sfdv4 = socket_create(rpv4))) {
		perror("error: setting up IPv4 socket");
		return sfd;
	}
	if (connect(sfdv4, rpv4->ai_addr, rpv4->ai_addrlen) != 0) {
		if (EINPROGRESS == errno) {
			fprintf(stderr, " in progress ... \n");
		} else {
			perror("error: connecting: ");
			close(sfdv4);
			return sfd;
		}
	}

	FD_ZERO(&readfds);
	FD_SET(sfd, &readfds);
	FD_SET(sfdv4, &readfds);
	writefds = readfds;
	errorfds = readfds;

	fprintf(stderr, "info: waiting for any socket ...\n");
	/* select with 300ms TO */
	if((ret = select(MAX(sfd,sfdv4)+1, &readfds, &writefds, &errorfds,
					NULL /* &timeout */)) < 0) {
		perror("error: second timeout");
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
		fprintf(stderr, "info: considering %s (%d) ... \n",
				result->ai_canonname,
				result->ai_family);
		if (AF_INET == result->ai_family) {
			return result;
		}
	}
	return NULL;
}

