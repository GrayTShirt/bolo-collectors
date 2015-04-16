#include "common.h"

#define HAVE_STDINT_H
#include <rrd.h>
#include <rrd_client.h>

static char *ADDRESS = NULL;
int parse_options(int argc, char **argv);

int main(int argc, char **argv)
{
	if (parse_options(argc, argv) != 0) {
		fprintf(stderr, "USAGE: %s [options]\n", argv[0]);
		exit(1);
	}

	rrdc_stats_t *head = NULL, *p;

	int rc = rrdc_connect(ADDRESS);
	if (rc != 0) exit(1);

	ts = time_s();
	rc = rrdc_stats_get(&head);
	assert(rc == 0);
	for (p = head; p; p = p->next) {
		printf("%s %i %s:rrdcache:%s %0.3f\n",
			p->type == RRDC_STATS_TYPE_GAUGE ? "SAMPLE" : "RATE",
			ts, PREFIX, p->name,
			p->type == RRDC_STATS_TYPE_GAUGE ? p->value.gauge : p->value.counter);
	}
}

int parse_options(int argc, char **argv)
{
	int errors = 0;

	int i;
	for (i = 1; i < argc; i++) {
		if (streq(argv[i], "-p") || streq(argv[i], "--prefix")) {
			if (++i >= argc) {
				fprintf(stderr, "Missing required value for -p\n");
				return 1;
			}
			PREFIX = strdup(argv[i]);
			continue;
		}

		if (streq(argv[i], "-h") || streq(argv[i], "-?") || streq(argv[i], "--help")) {
			fprintf(stdout, "rrdcache (a Bolo collector)\n"
			                "USAGE: rrdcache [options]\n"
			                "\n"
			                "options:\n"
			                "   -h, --help               Show this help screen\n"
			                "   -p, --prefix PREFIX      Use the given metric prefix\n"
			                "                            (FQDN is used by default)\n"
			                "   -S, --address SOCKET     Socket to connect to\n"
			                "                            Falls back to using the $RRDCACHED_ADDRESS\n"
			                "                            env var, or unix:/tmp/rrdcached.sock\n"
			                "\n");
			exit(0);
		}

		if (streq(argv[i], "-S") || streq(argv[i], "--address")) {
			if (++i >= argc) {
				fprintf(stderr, "Missing required value for -S\n");
				return 1;
			}
			ADDRESS = strdup(argv[i]);
		}

		fprintf(stderr, "Unrecognized argument '%s'\n", argv[i]);
		errors++;
	}
	if (!PREFIX) PREFIX = fqdn();
	if (!ADDRESS) ADDRESS = getenv("RRDCACHED_ADDRESS");
	if (!ADDRESS) ADDRESS = "unix:/tmp/rrdcached.sock";
	return errors;
}
