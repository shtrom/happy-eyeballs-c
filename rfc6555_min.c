/* Simple connection using getaddrinfo(3), from the manpage;
 * licensing terms for this function can be found at [0].
 *
 * [0] http://man7.org/linux/man-pages/man3/getaddrinfo.3.license.html
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "rfc6555.h"

int connect_host(char *host, char *service) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s;
	rfc6555_ctx *ctx;

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

	rfc6555_reorder(result);
	ctx = rfc6555_context_create();

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		fprintf(stderr, "connecting using rp %p (%s, af %d) ...",
				rp,
				rp->ai_canonname,
				rp->ai_family);
		sfd = socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		if (sfd == -1)
			continue;

		if ((sfd = rfc6555_connect(ctx, sfd, &rp)) != -1)
			break;                  /* Success */

		fprintf(stderr, " failed!\n");
		perror("error: connecting: ");
		close(sfd);
	}
	rfc6555_context_destroy(ctx);

	if (rp == NULL) {               /* No address succeeded */
		fprintf(stderr, "failed! (last attempt)\n");
		perror("error: connecting: ");
		return -3;
	}
	fprintf(stderr, " success!\n");

	freeaddrinfo(result);           /* No longer needed */

	return sfd;
}
