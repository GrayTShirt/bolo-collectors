#include "common.h"
#include <sys/socket.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>

#define PROC "/proc"

static char buf[8192];

typedef struct {
	char  *name;
	int    proto; /* SOCK_STREAM for tcp, SOCK_DGRAM for udp */
	int    af;    /* AF_INET or AF_INET6 */
	char  *process;

	void  *local_addr;
	void  *remote_addr;

	int local_port;
	int remote_port;

	unsigned long txq; /* cumulative tally */
	unsigned long rxq; /* cumulative tally */

	list_t l;
} alias_t;
static LIST(ALIASES);

int collect_tcp(void);
int collect_tcp6(void);
int collect_udp(void);
int collect_udp6(void);

static hash_t MASK = { 0 };
#define RUN_TAG  (void*)(1)
#define SKIP_TAG (void*)(2)
#define masked(s) (hash_get(&MASK, (s)) != NULL)
#define should(s) (hash_get(&MASK, (s)) == RUN_TAG)
#define RUN(s)     hash_set(&MASK, (s), RUN_TAG)
#define SKIP(s)    hash_set(&MASK, (s), SKIP_TAG)

static hash_t INODES = { 0 };

int parse_options(int argc, char **argv);
int addrcmp(int af, void *a, void *b);
int progcmp(const char *proc, const char *inode);
char* _readlink(const char *symlink);
char* _basename(const char *path);
int scan_proc_fd(void);
int push_alias(const char *spec);

int main (int argc, char **argv)
{
	if (parse_options(argc, argv) != 0) {
		fprintf(stderr, "USAGE: %s [-p prefix] [-l port,port,port] [-46tu]\n", argv[0]);
		exit(1);
	}

	if (scan_proc_fd() != 0) {
		fprintf(stderr, "Failed to scan /proc for socket -> program associations...\n");
		exit(1);
	}

	int rc = 0;
	#define TRY_STAT(rc,s) if (should(#s)) (rc) += collect_ ## s ()
	TRY_STAT(rc, tcp);
	TRY_STAT(rc, tcp6);
	TRY_STAT(rc, udp);
	TRY_STAT(rc, udp6);
	#undef TRY_STAT

	alias_t *alias;
	for_each_object(alias, &ALIASES, l) {
		printf("SAMPLE %i %s:netstat:%s:txqueue %lu\n", ts, PREFIX, alias->name, alias->txq);
		printf("SAMPLE %i %s:netstat:%s:rxqueue %lu\n", ts, PREFIX, alias->name, alias->rxq);
	}

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
				fprintf(stderr, "Missing required value for -p\n");
				return 1;
			}
			PREFIX = strdup(argv[i]);
			continue;
		}

		if (streq(argv[i], "-h") || streq(argv[i], "-?") || streq(argv[i], "--help")) {
			fprintf(stdout, "netstat (a Bolo collector)\n"
			                "USAGE: netstat [flags] [metrics]\n"
			                "\n"
			                "flags:\n"
			                "   -h, --help               Show this help screen\n"
			                "   -p, --prefix PREFIX      Use the given metric prefix\n"
			                "                            (FQDN is used by default)\n"
			                "\n"
			                "metrics:\n"
			                "\n"
			                "   (no)tcp           TCP/IPv4 socket metrics\n"
			                "   (no)tcp6          TCP/IPv6 socket metrics\n"
			                "   (no)udp           UDP/IPv4 socket metrics\n"
			                "   (no)udp6          UDP/IPv6 socket metrics\n"
			                "\n"
			                "   By default, all metrics are collected.  You can suppress specific\n"
			                "   metric sets by prefixing its name with \"no\", without having to\n"
			                "   list out everything you want explicitly.\n"
			                "\n");
			exit(0);
		}

		if (strchr(argv[i], '=')) {
			push_alias(argv[i]);
			continue;
		}

		#define KEYWORD(k,n) do { \
			if (streq(argv[i],      k)) {  RUN(n); nflagged++; continue; } \
			if (streq(argv[i], "no" k)) { SKIP(n);             continue; } \
		} while (0)

		KEYWORD("tcp",   "tcp");
		KEYWORD("tcp6",  "tcp6");
		KEYWORD("udp",   "udp");
		KEYWORD("udp6",  "udp6");

		#undef KEYWORD

		fprintf(stderr, "Unrecognized argument '%s'\n", argv[i]);
		errors++;
	}
	if (!PREFIX) PREFIX = fqdn();
	ts = time_s();

	if (nflagged == 0) {
		if (!masked("ipv4"))  RUN("ipv4");
		if (!masked("ipv6"))  RUN("ipv6");
		if (!masked("tcp"))   RUN("tcp");
		if (!masked("udp"))   RUN("udp");
	}
	return errors;
}

int addrcmp(int af, void *a, void *b)
{
	if (af == AF_INET)  return memcmp(a, b, sizeof(struct in_addr));
	if (af == AF_INET6) return memcmp(a, b, sizeof(struct in6_addr));
	return -1;
}

int progcmp(const char *proc, const char *inode)
{
	const char *real = hash_get(&INODES, inode);
	return real ? strcmp(real, proc) : -1;
}

char* _basename(const char *path)
{
	char *a = strrchr(path, '/');
	return strdup(a ? a + 1 : path);
}

char* _readlink(const char *symlink)
{
	char target[8192];
	ssize_t n = readlink(symlink, target, 8191);
	if (n < 0)
		return NULL;
	target[n] = '\0';
	return strdup(target);
}

int scan_proc_fd(void)
{
	/* strategy: go throught /proc/, looking for /proc/$PID/fd directories.
	   then, enumerate each proc/$PID/fd directory, trying to find symlinks
	   of the form 'socket:[(\d+)]' and use the inode string as a hash key
	   into INODES{} */

	DIR *proc = opendir(PROC);
	if (!proc)
		return 1;

	struct dirent *pid_d;
	while ((pid_d = readdir(proc)) != NULL) {
		if (!isdigit(pid_d->d_name[0]))
			continue; /* skip non-PID dirs */

		char *proc_fd_dir = string("%s/%s/fd", PROC, pid_d->d_name);
		DIR *proc_fd = opendir(proc_fd_dir);
		if (!proc_fd)
			continue;

		char *exe = string("%s/%s/exe", PROC, pid_d->d_name);
		char *prog_path = _readlink(exe); free(exe);
		if (!prog_path)
			continue;
		char *prog = _basename(prog_path); free(prog_path);

		struct dirent *fd_d;
		while ((fd_d = readdir(proc_fd)) != NULL) {
			char *link = string("%s/%s", proc_fd_dir, fd_d->d_name);
			char *target = _readlink(link);
			if (!target)
				continue;

			int rc;
			unsigned long inode;
			char *inode_hex;

			rc = sscanf(target, "socket:[%lu]", &inode);
			if (rc != 1)
				continue;

			inode_hex = string("%x", inode);
			hash_set(&INODES, inode_hex, prog);
		}
	}
}

int push_alias(const char *spec)
{
	struct {
		char  name[64];
		char  proto[5]; /* (tcp|udp)[46] */

		char  local_addr[46];
		char  local_port[6];

		char  remote_addr[46];
		char  remote_port[6];

		char  process[64];
	} RAW;
	char *end;
	int rc;

	rc = sscanf(spec, "%63[^=]=%[^:]:%45[^:]:%6[0-9*]-%[^:]:%6[0-9*]/%63s",
			RAW.name, RAW.proto,
			RAW.local_addr,  RAW.local_port,
			RAW.remote_addr, RAW.remote_port,
			RAW.process);
	if (rc != 7)
		return 1;

	alias_t *alias = vmalloc(sizeof(alias_t));
	list_init(&alias->l);

	alias->name = strdup(RAW.name);

	/* determine protocol and address family */
	if (strcmp(RAW.proto, "tcp") == 0) {
		alias->proto = SOCK_STREAM;
		alias->af    = AF_INET;

	} else if (strcmp(RAW.proto, "tcp6") == 0) {
		alias->proto = SOCK_STREAM;
		alias->af    = AF_INET6;

	} else if (strcmp(RAW.proto, "udp") == 0) {
		alias->proto = SOCK_DGRAM;
		alias->af    = AF_INET;

	} else if (strcmp(RAW.proto, "udp6") == 0) {
		alias->proto = SOCK_DGRAM;
		alias->af    = AF_INET6;

	} else {
		return 1;
	}

	/* determine local address */
	if (strcmp(RAW.local_addr, "*") == 0) {
		alias->local_addr = NULL;

	} else {
		alias->local_addr = vmalloc(sizeof(struct in6_addr)); /* largest */
		if (inet_pton(alias->af, RAW.local_addr, alias->local_addr) != 1)
			return 1;
	}

	/* determine local port */
	if (strcmp(RAW.local_port, "*") == 0) {
		alias->local_port = -1;

	} else {
		alias->local_port = strtol(RAW.local_port, &end, 10);
		if (*end)
			return 1;
	}

	/* determine remote address */
	if (strcmp(RAW.remote_addr, "*") == 0) {
		alias->remote_addr = NULL;

	} else {
		alias->remote_addr = vmalloc(sizeof(struct in6_addr)); /* largest */
		if (inet_pton(alias->af, RAW.remote_addr, alias->remote_addr) != 1)
			return 1;
	}

	/* determine remote port */
	if (strcmp(RAW.remote_port, "*") == 0) {
		alias->remote_port = -1;

	} else {
		alias->remote_port = strtol(RAW.remote_port, &end, 10);
		if (*end)
			return 1;
	}

	/* determine process name */
	if (strcmp(RAW.process, "*") == 0) {
		alias->process = NULL;

	} else {
		alias->process = strdup(RAW.process);
	}

	alias->txq = 0;
	alias->rxq = 0;

	list_push(&ALIASES, &alias->l);
	return 0;
}

int _collect_net(const char *path, int af, int proto)
{
	FILE *io = fopen(PROC "/net/tcp", "r");
	if (!io)
		return 1;

	struct {
		char          local_addr[33];   /* addresses are in hex notation */
		unsigned int  local_port;
		char          remote_addr[33];
		unsigned int  remote_port;
		unsigned int  state;
		unsigned long txq;
		unsigned long rxq;
		unsigned long inode;
	} N = {0};

	while (fgets(buf, 8192, io) != NULL) {
		/*
   6: 00000000:0CEA 00000000:0000 0A 00000000:00000000 00:00000000 00000000   498        0 2765463 1 ffff880219302e40 99 0 0 10 -1
		 */
		int rc = sscanf(buf, "%*d: %32[0-9A-Fa-f]:%x %32[0-9A-Fa-f]:%x %x %lx:%lx %*s %*s %*d %*d %lu",
				N.local_addr,  &N.local_port,
				N.remote_addr, &N.remote_port,
				&N.state, &N.txq, &N.rxq, &N.inode);
		if (rc != 8)
			continue;

		void *local_ip, *remote_ip;

		struct in_addr  local_ipv4  = {0};
		struct in_addr  remote_ipv4 = {0};
		struct in6_addr local_ipv6  = {0};
		struct in6_addr remote_ipv6 = {0};

		if (af == AF_INET) {
			sscanf(N.local_addr, "%x", &(local_ipv4.s_addr));
			sscanf(N.remote_addr, "%x", &(remote_ipv4.s_addr));

			local_ip  = &local_ipv4;
			remote_ip = &remote_ipv4;

		} else if (af == AF_INET6) {
			char a6[INET6_ADDRSTRLEN];
			struct in6_addr v6;

			sscanf(N.local_addr, "%08x%08x%08x%08x",
					&(v6.s6_addr32[0]), &(v6.s6_addr32[1]),
					&(v6.s6_addr32[2]), &(v6.s6_addr32[3]));
			inet_ntop(AF_INET6, &v6, a6, sizeof(a6));
			inet_pton(AF_INET6, a6, &local_ipv6);

			sscanf(N.remote_addr, "%08x%08x%08x%08x",
					&(v6.s6_addr32[0]), &(v6.s6_addr32[1]),
					&(v6.s6_addr32[2]), &(v6.s6_addr32[3]));
			inet_ntop(AF_INET6, &v6, a6, sizeof(a6));
			inet_pton(AF_INET6, a6, &remote_ipv6);

			local_ip  = &local_ipv6;
			remote_ip = &remote_ipv6;
		}

		char *inode_hex = string("%x", N.inode);

		alias_t *alias;
		for_each_object(alias, &ALIASES, l) {
			if (alias->af != af)
				continue;
			if (alias->proto != proto)
				continue;

			if (alias->local_addr && addrcmp(alias->af, alias->local_addr, local_ip) != 0)
				continue;
			if (alias->local_port > 0 && alias->local_port != N.local_port)
				continue;

			if (alias->remote_addr && addrcmp(alias->af, alias->remote_addr, remote_ip) != 0)
				continue;
			if (alias->remote_port > 0 && alias->remote_port != N.remote_port)
				continue;

			if (alias->process && progcmp(alias->process, inode_hex) != 0)
				continue;

			alias->txq += N.txq;
			alias->rxq += N.rxq;
		}
	}

	return 0;
}

int collect_tcp (void) { return _collect_net(PROC "/net/tcp",  AF_INET,  SOCK_STREAM); }
int collect_tcp6(void) { return _collect_net(PROC "/net/tcp6", AF_INET6, SOCK_STREAM); }
int collect_udp (void) { return _collect_net(PROC "/net/udp",  AF_INET,  SOCK_DGRAM);  }
int collect_udp6(void) { return _collect_net(PROC "/net/udp6", AF_INET6, SOCK_DGRAM);  }
