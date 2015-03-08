#include <stdio.h>
#include <unistd.h>
#include <vigor.h>
#include <string.h>

#define PROC "/proc"

static const char *PREFIX;

int collect_meminfo(void);
int collect_loadavg(void);
int collect_stat(void);

int main(int argc, char **argv)
{
	if (argc > 2) {
		fprintf(stderr, "USAGE: %s prefix\n", argv[0]);
		exit(1);
	}

	PREFIX = (argc == 2) ? argv[1] : fqdn();
	int rc = 0;

	rc += collect_meminfo();
	rc += collect_loadavg();
	rc += collect_stat();
	return rc;
}

int collect_meminfo(void)
{
	FILE *io = fopen(PROC "/meminfo", "r");
	if (!io)
		return 1;

	struct {
		uint64_t total;
		uint64_t used;
		uint64_t free;
		uint64_t buffers;
		uint64_t cached;
		uint64_t slab;
	} M = { 0 };
	struct {
		uint64_t total;
		uint64_t used;
		uint64_t free;
		uint64_t cached;
	} S = { 0 };
	uint64_t x;
	int32_t ts = time_s();
	char buf[8192];
	while (fgets(buf, 8192, io) != NULL) {
		/* MemTotal:        6012404 kB\n */
		char *k, *v, *u, *e;

		k = buf; v = strchr(k, ':');
		if (!v || !*v) continue;

		*v++ = '\0';
		while (isspace(*v)) v++;
		u = strchr(v, ' ');
		if (u) {
			*u++ = '\0';
		} else {
			u = strchr(v, '\n');
			if (u) *u = '\0';
			u = NULL;
		}

		x = strtoul(v, &e, 10);
		if (*e) continue;

		if (u && *u == 'k')
			x *= 1024;

		     if (strcmp(k, "MemTotal")   == 0) M.total   = x;
		else if (strcmp(k, "MemFree")    == 0) M.free    = x;
		else if (strcmp(k, "Buffers")    == 0) M.buffers = x;
		else if (strcmp(k, "Cached")     == 0) M.cached  = x;
		else if (strcmp(k, "Slab")       == 0) M.slab    = x;

		else if (strcmp(k, "SwapTotal")  == 0) S.total   = x;
		else if (strcmp(k, "SwapFree")   == 0) S.free    = x;
		else if (strcmp(k, "SwapCached") == 0) S.cached  = x;
	}

	M.used = M.total - (M.free + M.buffers + M.cached + M.slab);
	printf("SAMPLE %i %s:memory:total %lu\n",   ts, PREFIX, M.total);
	printf("SAMPLE %i %s:memory:used %lu\n",    ts, PREFIX, M.used);
	printf("SAMPLE %i %s:memory:free %lu\n",    ts, PREFIX, M.free);
	printf("SAMPLE %i %s:memory:buffers %lu\n", ts, PREFIX, M.buffers);
	printf("SAMPLE %i %s:memory:cached %lu\n",  ts, PREFIX, M.cached);
	printf("SAMPLE %i %s:memory:slab %lu\n",    ts, PREFIX, M.slab);

	S.used = S.total - (S.free + S.cached);
	printf("SAMPLE %i %s:swap:total %lu\n",   ts, PREFIX, S.total);
	printf("SAMPLE %i %s:swap:cached %lu\n",  ts, PREFIX, S.cached);
	printf("SAMPLE %i %s:swap:used %lu\n",    ts, PREFIX, S.used);
	printf("SAMPLE %i %s:swap:free %lu\n",    ts, PREFIX, S.free);

	fclose(io);
	return 0;
}

int collect_loadavg(void)
{
	FILE *io = fopen(PROC "/loadavg", "r");
	if (!io)
		return 1;

	int32_t ts = time_s();
	char buf[8192];
	if (fgets(buf, 8192, io) == NULL) {
		fclose(io);
		return 1;
	}

	char *a, *b;

	a = b = buf;
	while (*b && !isspace(*b)) b++; *b++ = '\0';
	printf("SAMPLE %i %s:load:1min %s\n", ts, PREFIX, a);

	while (isspace(*b)) b++; a = b;
	while (*b && !isspace(*b)) b++; *b++ = '\0';
	printf("SAMPLE %i %s:load:5min %s\n", ts, PREFIX, a);

	while (isspace(*b)) b++; a = b;
	while (*b && !isspace(*b)) b++; *b++ = '\0';
	printf("SAMPLE %i %s:load:15min %s\n", ts, PREFIX, a);

	while (*b && *b != '/') b++; a = ++b;
	while (*b && !isspace(*b)) b++; *b++ = '\0';
	printf("SAMPLE %i %s:processes:procs %s\n", ts, PREFIX, a);

	fclose(io);
	return 0;
}

int collect_stat(void)
{
	FILE *io = fopen(PROC "/stat", "r");
	if (!io)
		return 1;

	int cpus = 0;
	int32_t ts = time_s();
	char buf[8192];
	while (fgets(buf, 8192, io) != NULL) {
		char *k, *v, *p;

		k = v = buf;
		while (*v && !isspace(*v)) v++; *v++ = '\0';
		p = strchr(v, '\n'); if (p) *p = '\0';

		if (strcmp(k, "processes") == 0)
			printf("SAMPLE %i %s:processes:threads %s\n", ts, PREFIX, v);
		else if (strcmp(k, "procs_running") == 0)
			printf("SAMPLE %i %s:processes:running %s\n", ts, PREFIX, v);
		else if (strcmp(k, "procs_blocked") == 0)
			printf("SAMPLE %i %s:processes:blocked %s\n", ts, PREFIX, v);
		else if (strncmp(k, "cpu", 3) == 0 && isdigit(k[3]))
			cpus++;

		if (strcmp(k, "cpu") == 0) {
			while (*v && isspace(*v)) v++;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:user %s\n", ts, PREFIX, v); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:nice %s\n", ts, PREFIX, v); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:system %s\n", ts, PREFIX, v); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:idle %s\n", ts, PREFIX, v); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:iowait %s\n", ts, PREFIX, v); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:irq %s\n", ts, PREFIX, v); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:softirq %s\n", ts, PREFIX, v); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:steal %s\n", ts, PREFIX, v); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:guest %s\n", ts, PREFIX, v); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:guest-nice %s\n", ts, PREFIX, v); v = k;
		}
	}
	printf("SAMPLE %i %s:load:cpus %i\n", ts, PREFIX, cpus);

	fclose(io);
	return 0;
}
