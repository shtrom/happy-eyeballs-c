#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct app_config {
	char *host;
	int port;
};

int parse_argv(struct app_config *conf, int argc, char ** argv);

int main(int argc, char **argv) {
	struct app_config conf;
	int ret = 0;

	if (0 != (ret = parse_argv(&conf, argc, argv))) {
		fprintf(stderr, "error: %d\n", ret);
		fprintf(stderr, "usage: %s HOST PORT\n", argv[0]);
		return ret;
	}

	fprintf(stderr, "happy-eyeballing %s:%d ... \n", conf.host, conf.port);

	return ret;
}

int parse_argv(struct app_config *conf, int argc, char ** argv) {
	char *endptr;

	if (argc < 3) {
		return -1;
	}

	conf->host = strdup(argv[1]);
	conf->port = strtol(argv[2], &endptr, 10);
	if (endptr == argv[2]) {
		return -2;
	}

	return 0;
}
