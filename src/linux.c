#include <stdio.h>
#include <unistd.h>
#include <vigor.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define PROC "/proc"

static const char *PREFIX;

int collect_meminfo(void);
int collect_loadavg(void);
int collect_stat(void);
int collect_procs(void);
int collect_openfiles(void);
int collect_mounts(void);

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
	rc += collect_procs();
	rc += collect_openfiles();
	rc += collect_mounts();
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

	double load[3];
	uint64_t proc[3];

	int32_t ts = time_s();
	int rc = fscanf(io, "%lf %lf %lf %lu/%lu ",
			&load[0], &load[1], &load[2], &proc[0], &proc[1]);
	fclose(io);
	if (rc < 5)
		return 1;

	if (proc[0])
		proc[0]--; /* don't count us */

	printf("SAMPLE %i %s:load:1min"        " %0.2f\n",  ts, PREFIX, load[0]);
	printf("SAMPLE %i %s:load:5min"        " %0.2f\n",  ts, PREFIX, load[1]);
	printf("SAMPLE %i %s:load:15min"       " %0.2f\n",  ts, PREFIX, load[2]);
	printf("SAMPLE %i %s:load:runnable"    " %lu\n",    ts, PREFIX, proc[0]);
	printf("SAMPLE %i %s:load:schedulable" " %lu\n",    ts, PREFIX, proc[1]);
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
			printf("RATE %i %s:ctxt:forks-s %s\n", ts, PREFIX, v);
		else if (strcmp(k, "ctxt") == 0)
			printf("RATE %i %s:ctxt:cswch-s %s\n", ts, PREFIX, v);
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

int collect_procs(void)
{
	struct {
		uint16_t running;
		uint16_t sleeping;
		uint16_t zombies;
		uint16_t stopped;
		uint16_t paging;
		uint16_t blocked;
		uint16_t unknown;
	} P = {0};

	int pid;
	struct dirent *dir;
	DIR *d = opendir(PROC);
	if (!d)
		return 1;

	int32_t ts = time_s();
	while ((dir = readdir(d)) != NULL) {
		if (!isdigit(dir->d_name[0])
		 || (pid = atoi(dir->d_name)) < 1)
			continue;

		char *file = string(PROC "/%i/stat", pid);
		FILE *io = fopen(file, "r");
		free(file);
		if (!io)
			continue;

		char *a, buf[8192];
		if (!fgets(buf, 8192, io)) {
			fclose(io);
			continue;
		}
		fclose(io);

		a = buf;
		/* skip PID */
		while (*a && !isspace(*a)) a++;
		while (*a &&  isspace(*a)) a++;
		/* skip progname */
		while (*a && !isspace(*a)) a++;
		while (*a &&  isspace(*a)) a++;

		switch (*a) {
		case 'R': P.running++;  break;
		case 'S': P.sleeping++; break;
		case 'D': P.blocked++;  break;
		case 'Z': P.zombies++;  break;
		case 'T': P.stopped++;  break;
		case 'W': P.paging++;   break;
		default:  P.unknown++;  break;
		}
	}

	printf("SAMPLE %i %s:procs:running %i\n",  ts, PREFIX, P.running);
	printf("SAMPLE %i %s:procs:sleeping %i\n", ts, PREFIX, P.sleeping);
	printf("SAMPLE %i %s:procs:blocked %i\n",  ts, PREFIX, P.blocked);
	printf("SAMPLE %i %s:procs:zombies %i\n",  ts, PREFIX, P.zombies);
	printf("SAMPLE %i %s:procs:stopped %i\n",  ts, PREFIX, P.stopped);
	printf("SAMPLE %i %s:procs:paging %i\n",   ts, PREFIX, P.paging);
	printf("SAMPLE %i %s:procs:unknown %i\n",  ts, PREFIX, P.unknown);
	return 0;
}

int collect_openfiles(void)
{
	FILE *io = fopen(PROC "/sys/fs/file-nr", "r");
	if (!io)
		return 1;

	int32_t ts = time_s();
	char *a, *b, buf[8192];
	if (!fgets(buf, 8192, io)) {
		fclose(io);
		return 1;
	}

	a = buf;
	/* used file descriptors */
	while (*a &&  isspace(*a)) a++; b = a;
	while (*b && !isspace(*b)) b++; *b++ = '\0';
	printf("SAMPLE %i %s:openfiles:used %s\n", ts, PREFIX, a);

	a = b;
	/* free file descriptors */
	while (*a &&  isspace(*a)) a++; b = a;
	while (*b && !isspace(*b)) b++; *b++ = '\0';
	printf("SAMPLE %i %s:openfiles:free %s\n", ts, PREFIX, a);

	a = b;
	/* max file descriptors */
	while (*a &&  isspace(*a)) a++; b = a;
	while (*b && !isspace(*b)) b++; *b++ = '\0';
	printf("SAMPLE %i %s:openfiles:max %s\n", ts, PREFIX, a);

	return 0;
}

int collect_mounts(void)
{
	FILE *io = fopen(PROC "/mounts", "r");
	if (!io)
		return 1;

	struct stat st;
	struct statvfs fs;
	hash_t seen = {0};
	char *a, *b, *c, buf[8192];
	int32_t ts = time_s();
	while (fgets(buf, 8192, io) != NULL) {
		a = b = buf;
		for (b = buf; *b && !isspace(*b); b++); *b++ = '\0';
		for (c = b;   *c && !isspace(*c); c++); *c++ = '\0';
		char *dev = a, *path = b;

		if (hash_get(&seen, path))
			continue;
		hash_set(&seen, path, "1");

		if (lstat(path, &st) != 0
		 || statvfs(path, &fs) != 0
		 || !major(st.st_dev))
			continue;

		printf("KEY %s:fs:%s %s\n",  PREFIX, path, dev);
		printf("KEY %s:dev:%s %s\n", PREFIX, dev, path);

		printf("SAMPLE %i %s:df:%s:inodes.total %lu\n", ts, PREFIX, path, fs.f_files);
		printf("SAMPLE %i %s:df:%s:inodes.free %lu\n",  ts, PREFIX, path, fs.f_favail);
		printf("SAMPLE %i %s:df:%s:inodes.rfree %lu\n", ts, PREFIX, path, fs.f_ffree - fs.f_favail);

		printf("SAMPLE %i %s:df:%s:bytes.total %lu\n", ts, PREFIX, path, fs.f_frsize *  fs.f_blocks);
		printf("SAMPLE %i %s:df:%s:bytes.free %lu\n",  ts, PREFIX, path, fs.f_frsize *  fs.f_bavail);
		printf("SAMPLE %i %s:df:%s:bytes.rfree %lu\n", ts, PREFIX, path, fs.f_frsize * (fs.f_bfree - fs.f_bavail));
	}

	fclose(io);
	return 0;
}
