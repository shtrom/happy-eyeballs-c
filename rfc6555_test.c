#include <assert.h>
#include <malloc.h>

#include "rfc6555.h"

void test_context();

int main() {
	test_context();
}

#define MIN_CTX_LEN 4 /* Copied from the implementation */

void test_context() {
	rfc6555_ctx *ctx;

	ctx = rfc6555_context_create();
	assert(ctx != NULL);
	assert(malloc_usable_size(ctx) >= sizeof(rfc6555_ctx));
	assert(ctx->fds != NULL);
	assert(malloc_usable_size(ctx->fds) >= MIN_CTX_LEN * sizeof(int));
	assert(ctx->original_flags != NULL);
	assert(malloc_usable_size(ctx->original_flags) >= MIN_CTX_LEN * sizeof(int));
	assert(ctx->rps != NULL);
	assert(malloc_usable_size(ctx->rps) >= MIN_CTX_LEN * sizeof(struct addrinfo*));

	rfc6555_context_destroy(ctx);
}
