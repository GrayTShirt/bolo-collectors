#include "common.h"

#define HAVE_STDINT_H
#include <rrd.h>
#include <rrd_client.h>

int main(int argc, char **argv)
{
	PREFIX = fqdn();

	rrdc_stats_t *head = NULL, *p;

	char *daemon = getenv("RRDCACHED_ADDRESS");
	if (!daemon) daemon = "unix:/tmp/rrdcached.sock";

	int rc = rrdc_connect(daemon);
	assert(rc == 0);

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
