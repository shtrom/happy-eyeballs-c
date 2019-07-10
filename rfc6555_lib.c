#include <stdlib.h>
#include <netdb.h>

#include <fcntl.h>
#include <sys/select.h>

#include "rfc6555.h"

/* Minimal amount of entries to allocate in the context
 *
 * This is 2*2 (A + AAAA, doubled for wiggle room)
 */
#define MIN_CTX_SPACE 4

#define CONNECT_TIMEOUT_MS 300

static int rfc6555_context_append(rfc6555_ctx *ctx, int fd, struct addrinfo *rp, int flags);
static int rfc6555_context_grow(rfc6555_ctx *ctx);

rfc6555_ctx *rfc6555_context_create()
{
	rfc6555_ctx *ctx = malloc(sizeof(ctx));
	if (!ctx) {
		return NULL;
	}

	ctx->fds = NULL;
	ctx->rps = NULL;
	ctx->len = 0;
	ctx->max_len = MIN_CTX_SPACE;
	ctx->successful_fd = -1;

	if(rfc6555_context_grow(ctx) < 0) {
		rfc6555_context_destroy(ctx);
		return NULL;
	}

	return ctx;
}

/* Append an fd and associated rp to the context.
 * Return the index of the newly-added entries, or -1 on error */
static int rfc6555_context_append(rfc6555_ctx *ctx, int fd, struct addrinfo *rp, int flags)
{
	int idx;
	if(rfc6555_context_grow(ctx) < 0) {
		return -1;
	}

	idx = ctx->len;
	ctx->len++;

	ctx->fds[idx] = fd;
	ctx->original_flags[idx] = flags;
	ctx->rps[idx] = rp;

	return idx;
}

/* Double (or initialise) the size of the allocated storage if the max_len has
 * been reached.
 * Return -1 on error.
 */
#define ALLOC_FIELD(field, dtype, new_size) \
	do { \
		(field) = realloc((field), (new_size) * sizeof(dtype)); \
		if (!(field)) { \
			return -1; \
		} \
	} while(0)

static int rfc6555_context_grow(rfc6555_ctx *ctx)
{
	size_t new_size;

	if(!ctx) {
		return -1;
	}

	if(ctx->len < ctx->max_len) {
		return 0;
	}

	new_size = ctx->max_len * 2;
	if (new_size <= 0) {
		new_size = MIN_CTX_SPACE;
	}

	ALLOC_FIELD(ctx->fds, int, new_size);
	ALLOC_FIELD(ctx->original_flags, int, new_size);
	ALLOC_FIELD(ctx->rps, struct addrinfo*, new_size);

	return 0;
}

#define FREE_FIELD(field) \
	do { \
		if((field)) { \
			free((field)); \
		} \
	} while(0)

void rfc6555_context_destroy(rfc6555_ctx *ctx)
{
	if(!ctx) {
		return;
	};

	/* XXX: Probably some other stuff to cleanup here */

	FREE_FIELD(ctx->fds);
	FREE_FIELD(ctx->original_flags);
	FREE_FIELD(ctx->rps);

	free(ctx);
}

int rfc6555_reorder(struct addrinfo *result)
{
	int ret = -1;
	struct addrinfo *rp, *rp6 = NULL, *prev = NULL;

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		if (rp6 && AF_INET6 == rp->ai_family) {
			rp6 = rp;
		}
		if (AF_INET == rp->ai_family) {
			if (!rp6) {
				/* Found an IPv4 before an IPv6, don't mess up this weird order */
				return -1;
			}
			if (prev != rp6) {
				prev->ai_next = rp->ai_next;
				rp->ai_next = rp6->ai_next;
				rp6->ai_next = rp;
				ret = 0;
			}
			break;
		}
		prev = rp;
	}

	return ret;
}

int rfc6555_connect(rfc6555_ctx *ctx, int socket, struct addrinfo **rp)
{
	int fd = -1, maxfd = -1;
	int flags;
	int i;
	fd_set readfds, writefds, errorfds;
	struct timeval timeout = { 0, CONNECT_TIMEOUT_MS };

	flags = fcntl(socket, F_GETFL,0);
	rfc6555_context_append(ctx, socket, *rp, flags);

	fcntl(socket, F_SETFL, flags | O_NONBLOCK);

	FD_ZERO(&readfds);
	for(i=0; i<ctx->len; i++) {
		if(ctx->fds[i]<0) {
			continue;
		}
		FD_SET(ctx->fds[i], &readfds);
		if(ctx->fds[i] > maxfd) {
			maxfd = ctx->fds[i];
		}
	}
	writefds = readfds;
	errorfds = readfds;

	if(select(maxfd, &readfds, &writefds, &errorfds, &timeout) < 0)  {
		return -1;
	}

	for(i=0; i<ctx->len; i++) {
		if(FD_ISSET(ctx->fds[i], &readfds)
		   || FD_ISSET(ctx->fds[i], &writefds)
		) {
			fd = ctx->fds[i];
			break;
		} else if(FD_ISSET(ctx->fds[i], &errorfds)) {
			/* Neutralise erroneous fd */
			ctx->fds[i] = -1;
			ctx->original_flags[i] = -1;
			ctx->rps[i] = NULL;
		}
	}

	if (-1 != fd) {
		fcntl(fd, F_SETFL, ctx->original_flags[i]);
		*rp = ctx->rps[i];
	}
	return fd;
}
