#include <assert.h>

#include "rfc6555.h"

void test_context();

int main() {
	test_context();
}

void test_context() {
	rfc6555_ctx *ctx;

	ctx = rfc6555_context_create();
	assert(ctx != NULL);
	assert(ctx->fds != NULL);
	assert(ctx->original_flags != NULL);
	assert(ctx->rps != NULL);

	rfc6555_context_destroy(ctx);
}
