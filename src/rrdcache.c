#include <assert.h>
#include <stdio.h>
#include <vigor.h>

#define HAVE_STDINT_H
#include <rrd.h>
#include <rrd_client.h>

static const char *PREFIX;
static int32_t ts;

int main(int argc, char **argv)
{
	PREFIX = fqdn();

	rrdc_stats_t *head = NULL, *p;
	int rc = rrdc_connect("unix:/tmp/rrdcached.sock");
	assert(rc == 0);

	ts = time_s();
	rc = rrdc_stats_get(&head);
	assert(rc == 0);
	for (p = head; p; p = p->next) {
		printf("%s %i %s:rrdcache:%s %e\n",
			p->type == RRDC_STATS_TYPE_GAUGE ? "SAMPLE" : "RATE",
			ts, PREFIX, p->name,
			p->type == RRDC_STATS_TYPE_GAUGE ? p->value.gauge : p->value.counter);
	}
}
