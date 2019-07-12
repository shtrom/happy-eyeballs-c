#  Simple C implementation of RFC-6555 (Happy Eyeballs)

This is an implementation of the Happy Eyeballs algorithm from [rfc6555].

The aim is to provide a (almost) drop-in replacement to the standard [`connect`(3)]
system call, so it can be used in the Example loop from [`getaddrinfo`(3)], for
ease of integration in existing projects.


## Get it

The [authoritative source for this project can be found
here](https://scm.narf.ssji.net/git/happy-eyeballs-c/), but [mirror is
maintained in sync on GitHub](https://github.com/shtrom/happy-eyeballs-c).
You can clone the latest version with either

    git clone https://scm.narf.ssji.net/git/happy-eyeballs-c

or

    git clone https://github.com/shtrom/happy-eyeballs-c


## Try it

Just run

    make test

to build and run various combinations of tests (pay attention to the names of
the binaries, as well as the amount of time elapsed).


## Use it

As stated above, the aim is to have to make as few changes as possible in the
Example GAI loop. Here's a diff between `gai.c` and `rfc6555_min.c` at the time
of this writing.

```diff
--- gai.c	2019-07-10 21:39:59.827667939 +1000
+++ happy.c	2019-07-12 17:15:06.288931156 +1000
@@ -12,10 +12,13 @@
 #include <netdb.h>
 #include <unistd.h>
 
+#include "rfc6555.h"
+
 int connect_host(char *host, char *service) {
 	struct addrinfo hints;
 	struct addrinfo *result, *rp;
 	int sfd, s;
+	rfc6555_ctx *ctx;
 
 	memset(&hints, 0, sizeof(struct addrinfo));
 	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
@@ -34,6 +37,9 @@
 	   If socket(2) (or connect(2)) fails, we (close the socket
 	   and) try the next address. */
 
+	rfc6555_reorder(result);
+	ctx = rfc6555_context_create();
+
 	for (rp = result; rp != NULL; rp = rp->ai_next) {
 		fprintf(stderr, "connecting using rp %p (%s, af %d) ...",
 				rp,
@@ -44,13 +50,13 @@
 		if (sfd == -1)
 			continue;
 
-		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
+		if ((sfd = rfc6555_connect(ctx, sfd, &rp)) != -1)
 			break;                  /* Success */
 
 		fprintf(stderr, " failed!\n");
 		perror("error: connecting: ");
-		close(sfd);
 	}
+	rfc6555_context_destroy(ctx);
 
 	if (rp == NULL) {               /* No address succeeded */
 		fprintf(stderr, "failed! (last attempt)\n");
```

## TODO

* Review and (maybe) implement [rfc8305];
* Make this an actual library with autoconf and all.


## Authors

This implementation:
* Olivier Mehani <shtrom@ssji.net>, 2019

The [`getaddrinfo`(3)] Example code:
* Sam Varshavchik <mrsam@courier-mta.com>, 2000
* Ulrich Drepper <drepper@redhat.com>, 2006
* Michael Kerrisk <mtk.manpages@gmail.com>, 2007, 2008


## Licenses

The drop-in file and the accompanying headers are under LGPL-3.0-or-later; the
rest is under GPL-3.0-or-later, except for the Example code from
[`getaddrinfo`(3)], for which terms can be found at [gai-example-license].


[rfc6555]: https://tools.ietf.org/rfcmarkup/6555
[rfc8305]: https://tools.ietf.org/rfcmarkup/8305
[`connect`(3)]: https://linux.die.net/man/3/connect
[`getaddrinfo`(3)]: https://linux.die.net/man/3/getaddrinfo
[gai-example-license]: http://man7.org/linux/man-pages/man3/getaddrinfo.3.license.html
