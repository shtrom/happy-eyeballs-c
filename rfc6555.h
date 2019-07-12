#ifndef __RFC6555_H

#include <unistd.h>
#include <sys/types.h>

typedef struct {
	int* fds;
	int* original_flags;
	struct addrinfo* *rps;
	size_t len;
	size_t max_len;
	int successful_fd;
} rfc6555_ctx;

/* Create context */
rfc6555_ctx *rfc6555_context_create();
/* Destroy context and cleanup resources, except for successful socket, if any */
void rfc6555_context_destroy(rfc6555_ctx *ctx);

/* Loop through result, and place the first-found af_inet entry just after
 * the first af_inet6 entry.
 * Return 0 if this happened, -1 otherwise.
 */
int rfc6555_reorder(struct addrinfo *result);

/* Add a new socket, for rp, to the list, and perform a new select().
 * Return the first socket to successfully connect, or -1 otherwise.
 * Additionally, on successful connection, the rp pointer is updated
 * to match the returned socket.
 */
int rfc6555_connect(rfc6555_ctx *ctx, int sockfd, struct addrinfo **rp);

#endif /*__RFC6555_H */
