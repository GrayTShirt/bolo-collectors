#include "common.h"
#include <dirent.h>
#include <sys/statvfs.h>
#include <pcre.h>

#define PROC "/proc"

static char buf[8192];

#define MATCH_ANY   0
#define MATCH_IFACE 1
#define MATCH_MOUNT 2
#define MATCH_DEV   3

typedef struct __matcher {
	int         subject; /* what type of thing to match (a MATCH_* const) */
	char       *pattern; /* raw pattern source string.                    */
	pcre       *regex;   /* compiled pattern to match against.            */
	pcre_extra *extra;   /* additional stuff from pcre_study (perf tweak) */

	struct __matcher *next;
} matcher_t;
static matcher_t *EXCLUDE = NULL;
static matcher_t *INCLUDE = NULL;

int collect_meminfo(void);
int collect_loadavg(void);
int collect_stat(void);
int collect_procs(void);
int collect_openfiles(void);
int collect_mounts(void);
int collect_vmstat(void);
int collect_diskstats(void);
int collect_netdev(void);

static hash_t MASK = { 0 };
#define RUN_TAG  (void*)(1)
#define SKIP_TAG (void*)(2)
#define masked(s) (hash_get(&MASK, (s)) != NULL)
#define should(s) (hash_get(&MASK, (s)) == RUN_TAG)
#define RUN(s)     hash_set(&MASK, (s), RUN_TAG)
#define SKIP(s)    hash_set(&MASK, (s), SKIP_TAG)

int parse_options(int argc, char **argv);
int matches(int type, const char *name);
int append_matcher(matcher_t **root, const char *flag, const char *value);

int main(int argc, char **argv)
{
	if (parse_options(argc, argv) != 0) {
		fprintf(stderr, "USAGE: %s [-p prefix]\n", argv[0]);
		exit(1);
	}

	int rc = 0;
	#define TRY_STAT(rc,s) if (should(#s)) (rc) += collect_ ## s ()
	TRY_STAT(rc, meminfo);
	TRY_STAT(rc, loadavg);
	TRY_STAT(rc, stat);
	TRY_STAT(rc, procs);
	TRY_STAT(rc, openfiles);
	TRY_STAT(rc, mounts);
	TRY_STAT(rc, vmstat);
	TRY_STAT(rc, diskstats);
	TRY_STAT(rc, netdev);
	#undef TRY_STAT
	return rc;
}

int parse_options(int argc, char **argv)
{
	int errors = 0;
	int nflagged = 0;

	int i;
	for (i = 1; i < argc; ++i) {
		if (streq(argv[i], "-p") || streq(argv[i], "--prefix")) {
			if (++i >= argc) {
				fprintf(stderr, "Missing required value for --prefix flag\n");
				return 1;
			}
			PREFIX = strdup(argv[i]);
			continue;
		}

		if (streq(argv[i], "-i") || streq(argv[i], "--include")) {
			if (++i >= argc) {
				fprintf(stderr, "Missing required value for --include flag\n");
				return 1;
			}
			if (append_matcher(&INCLUDE, "--include", argv[i]) != 0) {
				return 1;
			}
			continue;
		}

		if (streq(argv[i], "-x") || streq(argv[i], "--exclude")) {
			if (++i >= argc) {
				fprintf(stderr, "Missing required value for --exclude flag\n");
				return 1;
			}
			if (append_matcher(&EXCLUDE, "--exclude", argv[i]) != 0) {
				return 1;
			}
			continue;
		}

		if (streq(argv[i], "-h") || streq(argv[i], "-?") || streq(argv[i], "--help")) {
			fprintf(stdout, "linux (a Bolo collector)\n"
			                "USAGE: linux [flags] [metrics]\n"
			                "\n"
			                "flags:\n"
			                "   -h, --help                 Show this help screen\n"
			                "   -p, --prefix PREFIX        Use the given metric prefix\n"
			                "                              (FQDN is used by default)\n"
			                "\n"
			                "   -i, --include type:regex   Only consider things of the given type\n"
			                "                              that match /^regex$/, using PCRE.\n"
			                "                              By default, all things are included.\n"
			                "\n"
			                "   -x, --exclude type:regex   Don't consider things (of the given type)\n"
			                "                              that match /^regex$/, using PCRE.\n"
			                "                              By default, nothing is excluded.\n"
			                "\n"
			                "                              Note: --exclude rules are processed after\n"
			                "                              all --include rules, so you usually want\n"
			                "                              to match liberally first, and exclude\n"
			                "                              conservatively.\n"
			                "\n"
			                "metrics:\n"
			                "\n"
			                "   (no)mem           Memory utilization metrics\n"
			                "   (no)load          System Load Average metrics\n"
			                "   (no)cpu           CPU utilization (aggregate) metrics\n"
			                "   (no)procs         Process creation / context switching metrics\n"
			                "   (no)openfiles     Open File Descriptor metrics\n"
			                "   (no)mounts        Mountpoints and disk space usage metrics\n"
			                "   (no)paging        Virtual Memory paging statistics\n"
			                "   (no)disk          Disk I/O and utilization metrics\n"
			                "   (no)net           Network interface metrics\n"
			                "\n"
			                "   By default, all metrics are collected.  You can suppress specific\n"
			                "   metric sets by prefixing its name with \"no\", without having to\n"
			                "   list out everything you want explicitly.\n"
			                "\n");
			exit(0);
		}

		int good = 0;
		#define KEYWORD(k,n) do { \
			if (streq(argv[i],      k)) {  RUN(n); nflagged++; good = 1; continue; } \
			if (streq(argv[i], "no" k)) { SKIP(n);             good = 1; continue; } \
		} while (0)

		KEYWORD("mem",       "meminfo");
		KEYWORD("load",      "loadavg");
		KEYWORD("cpu",       "stat");
		KEYWORD("procs",     "procs");
		KEYWORD("openfiles", "openfiles");
		KEYWORD("mounts",    "mounts");
		KEYWORD("paging",    "vmstat");
		KEYWORD("disk",      "diskstats");
		KEYWORD("net",       "netdev");

		#undef KEYWORD
		if (good) continue;

		fprintf(stderr, "Unrecognized argument '%s'\n", argv[i]);
		errors++;
	}

	INIT_PREFIX();

	if (nflagged == 0) {
		if (!masked("meminfo"))   RUN("meminfo");
		if (!masked("loadavg"))   RUN("loadavg");
		if (!masked("stat"))      RUN("stat");
		if (!masked("procs"))     RUN("procs");
		if (!masked("openfiles")) RUN("openfiles");
		if (!masked("mounts"))    RUN("mounts");
		if (!masked("vmstat"))    RUN("vmstat");
		if (!masked("diskstats")) RUN("diskstats");
		if (!masked("netdev"))    RUN("netdev");
	}
	return errors;
}

int matches(int type, const char *name)
{
	matcher_t *m;

	if ((m = INCLUDE) != NULL) {
		while (m) {
			if ((m->subject == MATCH_ANY || m->subject == type)
			 && pcre_exec(m->regex, m->extra, name, strlen(name), 0, 0, NULL, 0) == 0) {
				goto excludes;
			}
			m = m->next;
		}
		return 0; /* not included */
	}

excludes:
	m = EXCLUDE;
	while (m) {
		if ((m->subject == MATCH_ANY || m->subject == type)
		 && pcre_exec(m->regex, m->extra, name, strlen(name), 0, 0, NULL, 0) == 0) {
			return 0; /* excluded */
		}
		m = m->next;
	}

	return 1;
}

int append_matcher(matcher_t **root, const char *flag, const char *value)
{
	matcher_t *m;
	const char *colon, *re_err;
	int re_off;

	m = calloc(1, sizeof(matcher_t));
	if (!m) {
		fprintf(stderr, "unable to allocate memory: %s (errno %d)\n", strerror(errno), errno);
		exit(1);
	}

	m->subject = MATCH_ANY;
	if ((colon = strchr(value, ':')) != NULL) {
		if (strncasecmp(value, "iface:", colon - value) == 0) {
			m->subject = MATCH_IFACE;
		} else if (strncasecmp(value, "dev:", colon - value) == 0) {
			m->subject = MATCH_DEV;
		} else if (strncasecmp(value, "mount:", colon - value) == 0) {
			m->subject = MATCH_MOUNT;
		} else {
			fprintf(stderr, "Invalid type in type:regex specifier for %s flag\n", flag);
			free(m);
			return 1;
		}
		value = colon + 1;
	}

	if (!*value) {
		fprintf(stderr, "Missing regex in type:regex specifier for %s flag\n", flag);
		free(m);
		return 1;
	}

	m->pattern = calloc(1 + strlen(value) + 1 + 1, sizeof(char));
	if (!m->pattern) {
		fprintf(stderr, "unable to allocate memory: %s (errno %d)\n", strerror(errno), errno);
		exit(1);
	}
	m->pattern[0] = '^';
	memcpy(m->pattern+1, value, strlen(value));
	m->pattern[1+strlen(value)] = '$';

	m->regex = pcre_compile(m->pattern, PCRE_ANCHORED, &re_err, &re_off, NULL);
	if (!m->regex) {
		fprintf(stderr, "Bad regex '%s' (error %s) for %s flag\n", m->pattern, flag, re_err);
		free(m->pattern);
		free(m);
		return 1;
	}
	m->extra = pcre_study(m->regex, 0, &re_err);

	if (!*root) {
		*root = m;
	} else {
		while (root && (*root)->next) {
			root = &(*root)->next;
		}
		(*root)->next = m;
	}
	return 0;
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
	ts = time_s();
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

		     if (streq(k, "MemTotal"))   M.total   = x;
		else if (streq(k, "MemFree"))    M.free    = x;
		else if (streq(k, "Buffers"))    M.buffers = x;
		else if (streq(k, "Cached"))     M.cached  = x;
		else if (streq(k, "Slab"))       M.slab    = x;

		else if (streq(k, "SwapTotal"))  S.total   = x;
		else if (streq(k, "SwapFree"))   S.free    = x;
		else if (streq(k, "SwapCached")) S.cached  = x;
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

	ts = time_s();
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
	ts = time_s();
	while (fgets(buf, 8192, io) != NULL) {
		char *k, *v, *p;

		k = v = buf;
		while (*v && !isspace(*v)) v++; *v++ = '\0';
		p = strchr(v, '\n'); if (p) *p = '\0';

		if (streq(k, "processes"))
			printf("RATE %i %s:ctxt:forks-s %s\n", ts, PREFIX, v);
		else if (streq(k, "ctxt"))
			printf("RATE %i %s:ctxt:cswch-s %s\n", ts, PREFIX, v);
		else if (strncmp(k, "cpu", 3) == 0 && isdigit(k[3]))
			cpus++;

		if (streq(k, "cpu")) {
			while (*v && isspace(*v)) v++;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:user %s\n", ts, PREFIX, v && *v ? v : "0"); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:nice %s\n", ts, PREFIX, v && *v ? v : "0"); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:system %s\n", ts, PREFIX, v && *v ? v : "0"); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:idle %s\n", ts, PREFIX, v && *v ? v : "0"); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:iowait %s\n", ts, PREFIX, v && *v ? v : "0"); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:irq %s\n", ts, PREFIX, v && *v ? v : "0"); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:softirq %s\n", ts, PREFIX, v && *v ? v : "0"); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:steal %s\n", ts, PREFIX, v && *v ? v : "0"); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:guest %s\n", ts, PREFIX, v && *v ? v : "0"); v = k;
			k = v; while (*k && !isspace(*k)) k++; *k++ = '\0';
			printf("RATE %i %s:cpu:guest-nice %s\n", ts, PREFIX, v && *v ? v : "0"); v = k;
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

	ts = time_s();
	while ((dir = readdir(d)) != NULL) {
		if (!isdigit(dir->d_name[0])
		 || (pid = atoi(dir->d_name)) < 1)
			continue;

		char *file = string(PROC "/%i/stat", pid);
		FILE *io = fopen(file, "r");
		free(file);
		if (!io)
			continue;

		char *a;
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

	ts = time_s();
	char *a, *b;
	if (!fgets(buf, 8192, io)) {
		fclose(io);
		return 1;
	}

	a = buf;
	/* used file descriptors */
	while (*a &&  isspace(*a)) a++; b = a;
	while (*b && !isspace(*b)) b++; *b++ = '\0';
	printf("SAMPLE %i %s:openfiles:used %s\n", ts, PREFIX, a && *a ? a : "0");

	a = b;
	/* free file descriptors */
	while (*a &&  isspace(*a)) a++; b = a;
	while (*b && !isspace(*b)) b++; *b++ = '\0';
	printf("SAMPLE %i %s:openfiles:free %s\n", ts, PREFIX, a && *a ? a : "0");

	a = b;
	/* max file descriptors */
	while (*a &&  isspace(*a)) a++; b = a;
	while (*b && !isspace(*b)) b++; *b++ = '\0';
	printf("SAMPLE %i %s:openfiles:max %s\n", ts, PREFIX, a && *a ? a : "0");

	return 0;
}

char* resolv_path(char *path)
{
	char  *buf  = malloc(256);
	if (buf == NULL)
		return path;
	ssize_t size = 0;
	char *dev  = strdup(path);

	if ((size = readlink(dev, buf, 256)) == -1)
		return path;
	buf[size] = '\0';

	int begin = 0;
	int cnt   = 1;
	while((begin = strspn(buf, "..")) != 0) {
		buf += begin;
		cnt++;
	}
	int i;
	for (i = 0; i < cnt; i++)
		dev[strlen(dev) - strlen(strrchr(dev, '/'))] = '\0';
	strcat(dev, buf);
	return dev;
}

int collect_mounts(void)
{
	FILE *io = fopen(PROC "/mounts", "r");
	if (!io)
		return 1;

	struct stat st;
	struct statvfs fs;
	hash_t seen = {0};
	char *a, *b, *c;
	ts = time_s();
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

		if (!matches(MATCH_MOUNT, path))
			continue;
		dev = resolv_path(dev);

		printf("KEY %s:fs:%s\n",  PREFIX, path);
		printf("KEY %s:dev:%s\n",  PREFIX, dev);
		printf("KEY %s:fs2dev:%s=%s\n",  PREFIX, path, dev);
		printf("KEY %s:dev2fs:%s=%s\n", PREFIX, dev, path);

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

int collect_vmstat(void)
{
	FILE *io = fopen(PROC "/vmstat", "r");
	if (!io)
		return 1;

	uint64_t pgsteal = 0;
	uint64_t pgscan_kswapd = 0;
	uint64_t pgscan_direct = 0;
	ts = time_s();
	while (fgets(buf, 8192, io) != NULL) {
		char name[64];
		uint64_t value;
		int rc = sscanf(buf, "%63s %lu\n", name, &value);
		if (rc < 2)
			continue;

#define VMSTAT_SIMPLE(x,n,v,t) do { \
	if (streq((n), #t)) printf("RATE %i %s:vm:%s %lu\n", ts, PREFIX, #t, (v)); \
} while (0)
		VMSTAT_SIMPLE(VM, name, value, pswpin);
		VMSTAT_SIMPLE(VM, name, value, pswpout);
		VMSTAT_SIMPLE(VM, name, value, pgpgin);
		VMSTAT_SIMPLE(VM, name, value, pgpgout);
		VMSTAT_SIMPLE(VM, name, value, pgfault);
		VMSTAT_SIMPLE(VM, name, value, pgmajfault);
		VMSTAT_SIMPLE(VM, name, value, pgfree);
#undef  VMSTAT_SIMPLE

		if (strncmp(name, "pgsteal_", 8) == 0)        pgsteal       += value;
		if (strncmp(name, "pgscan_kswapd_", 14) == 0) pgscan_kswapd += value;
		if (strncmp(name, "pgscan_direct_", 14) == 0) pgscan_direct += value;
	}
	printf("RATE %i %s:vm:pgsteal %lu\n",       ts, PREFIX, pgsteal);
	printf("RATE %i %s:vm:pgscan.kswapd %lu\n", ts, PREFIX, pgscan_kswapd);
	printf("RATE %i %s:vm:pgscan.direct %lu\n", ts, PREFIX, pgscan_direct);

	fclose(io);
	return 0;
}

/* FIXME: figure out a better way to detect devices */
#define is_device(dev) (strncmp((dev), "loop", 4) != 0 && strncmp((dev), "ram", 3) != 0)

int collect_diskstats(void)
{
	FILE *io = fopen(PROC "/diskstats", "r");
	if (!io)
		return 1;

	uint32_t dev[2];
	uint64_t rd[4], wr[4];
	ts = time_s();
	while (fgets(buf, 8192, io) != NULL) {
		char name[32];
		int rc = sscanf(buf, "%u %u %31s %lu %lu %lu %lu %lu %lu %lu %lu",
				&dev[0], &dev[1], name,
				&rd[0], &rd[1], &rd[2], &rd[3],
				&wr[0], &wr[1], &wr[2], &wr[3]);
		if (rc != 11)
			continue;
		if (!is_device(name))
			continue;

		if (!matches(MATCH_DEV, name))
			continue;

		printf("RATE %i %s:diskio:%s:rd-iops %lu\n",  ts, PREFIX, name, rd[0]);
		printf("RATE %i %s:diskio:%s:rd-miops %lu\n", ts, PREFIX, name, rd[1]);
		printf("RATE %i %s:diskio:%s:rd-bytes %lu\n", ts, PREFIX, name, rd[2] * 512);
		printf("RATE %i %s:diskio:%s:rd-msec %lu\n",  ts, PREFIX, name, rd[3]);

		printf("RATE %i %s:diskio:%s:wr-iops %lu\n",  ts, PREFIX, name, wr[0]);
		printf("RATE %i %s:diskio:%s:wr-miops %lu\n", ts, PREFIX, name, wr[1]);
		printf("RATE %i %s:diskio:%s:wr-bytes %lu\n", ts, PREFIX, name, wr[2] * 512);
		printf("RATE %i %s:diskio:%s:wr-msec %lu\n",  ts, PREFIX, name, wr[3]);
	}

	fclose(io);
	return 0;
}

int collect_netdev(void)
{
	FILE *io = fopen(PROC "/net/dev", "r");
	if (!io)
		return 1;

	ts = time_s();
	if (fgets(buf, 8192, io) == NULL
	 || fgets(buf, 8192, io) == NULL) {
		fclose(io);
		return 1;
	}

	struct {
		uint64_t bytes;
		uint64_t packets;
		uint64_t errors;
		uint64_t drops;
		uint64_t overruns;
		uint64_t frames;
		uint64_t compressed;
		uint64_t collisions;
		uint64_t multicast;
		uint64_t carrier;
	} tx = {0xff}, rx = {0xff};

	while (fgets(buf, 8192, io) != NULL) {
		char *x = strrchr(buf, ':');
		if (x) *x = ' ';

		char name[32];
		int rc = sscanf(buf, " %31s "
			"%lu %lu %lu %lu %lu %lu %lu %lu "
			"%lu %lu %lu %lu %lu %lu %lu %lu\n",
			name,
			&rx.bytes, &rx.packets, &rx.errors, &rx.drops,
			&rx.overruns, &rx.frames, &rx.compressed, &rx.multicast,
			&tx.bytes, &tx.packets, &tx.errors, &tx.drops,
			&tx.overruns, &tx.collisions, &tx.carrier, &tx.compressed);

		if (rc < 17)
			continue;

		if (!matches(MATCH_IFACE, name))
			continue;

		printf("RATE %i %s:net:%s:rx.bytes %lu\n",      ts, PREFIX, name, rx.bytes);
		printf("RATE %i %s:net:%s:rx.packets %lu\n",    ts, PREFIX, name, rx.packets);
		printf("RATE %i %s:net:%s:rx.errors %lu\n",     ts, PREFIX, name, rx.errors);
		printf("RATE %i %s:net:%s:rx.drops %lu\n",      ts, PREFIX, name, rx.drops);
		printf("RATE %i %s:net:%s:rx.overruns %lu\n",   ts, PREFIX, name, rx.overruns);
		printf("RATE %i %s:net:%s:rx.compressed %lu\n", ts, PREFIX, name, rx.compressed);
		printf("RATE %i %s:net:%s:rx.frames %lu\n",     ts, PREFIX, name, rx.frames);
		printf("RATE %i %s:net:%s:rx.multicast %lu\n",  ts, PREFIX, name, rx.multicast);

		printf("RATE %i %s:net:%s:tx.bytes %lu\n",      ts, PREFIX, name, tx.bytes);
		printf("RATE %i %s:net:%s:tx.packets %lu\n",    ts, PREFIX, name, tx.packets);
		printf("RATE %i %s:net:%s:tx.errors %lu\n",     ts, PREFIX, name, tx.errors);
		printf("RATE %i %s:net:%s:tx.drops %lu\n",      ts, PREFIX, name, tx.drops);
		printf("RATE %i %s:net:%s:tx.overruns %lu\n",   ts, PREFIX, name, tx.overruns);
		printf("RATE %i %s:net:%s:tx.compressed %lu\n", ts, PREFIX, name, tx.compressed);
		printf("RATE %i %s:net:%s:tx.collisions %lu\n", ts, PREFIX, name, tx.collisions);
		printf("RATE %i %s:net:%s:tx.carrier %lu\n",    ts, PREFIX, name, tx.carrier);
	}

	fclose(io);
	return 0;
}
