#include "common.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <pthread.h>

struct {
	int    timeout;
	char  *host;
	char **ports;
	int    nports;
} OPTIONS = {
	.timeout = 2,
	.host    = NULL,
	.ports   = NULL,
	.nports  = 0,
};

pthread_barrier_t barrier;

int parse_options(int argc, char **argv);
int resolve_ipv4(struct sockaddr_in *sa, const char *addr);

void* signal_thread(void*);
void* worker_thread(void*);

int main(int argc, char **argv)
{
	int rc;
	if (parse_options(argc, argv) != 0) {
		fprintf(stderr, "USAGE: %s [options] port [port ...]\n", argv[0]);
		exit(1);
	}
	assert(OPTIONS.ports[0]);

	rc = pthread_barrier_init(&barrier, NULL, OPTIONS.nports + 1);
	if (rc != 0) {
		perror("pthread_barrier_init");
		exit(2);
	}

	struct sockaddr_in sa = { 0 };
	resolve_ipv4(&sa, OPTIONS.host);

	pthread_t sig_tid;
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGALRM);
	rc = pthread_sigmask(SIG_BLOCK, &set, NULL);
	if (rc != 0) {
		errno = rc;
		perror("pthread_signmask");
		exit(1);
	}
	pthread_create(&sig_tid, NULL, signal_thread, &set);

	stopwatch_t w;
	int ms;

	int i;
	for (i = 0; OPTIONS.ports[i]; i++) {
		printf("KEY %i %s:tcp:%s\n", ts, PREFIX, OPTIONS.ports[i]);

		struct sockaddr_in *copy = calloc(1, sizeof(struct sockaddr_in));
		memcpy(copy, &sa, sizeof(sa));
		copy->sin_port = htons(atoi(OPTIONS.ports[i]));

		pthread_t tid;
		pthread_create(&tid, NULL, worker_thread, copy);
		copy = NULL;
	}
	alarm(OPTIONS.timeout);

	pthread_barrier_wait(&barrier);
	pthread_cancel(sig_tid);
	pthread_exit(NULL);
}

void* signal_thread(void *u)
{
	sigset_t *set = (sigset_t*)u;
	int s, sig;

	for (;;) {
		s = sigwait(set, &sig);
		if (s != 0) {
			errno = s;
			perror("sigwait");
			exit(1);
		}

		// bail on signal ALRM
		exit(1);
	}
}

void* worker_thread(void *u)
{
	int rc;
	struct sockaddr_in *sa = (struct sockaddr_in*)u;

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		fprintf(stderr, "socket() failed: (%i) %s\n", errno, strerror(errno));
		goto done;
	}

	stopwatch_t w;
	int ms;

	STOPWATCH(&w, ms) { rc = connect(fd, (struct sockaddr*)sa, sizeof(struct sockaddr_in)); }

	if (rc != 0) {
		fprintf(stderr, "Failed to connect to %s:%i: %s (%i)\n",
				OPTIONS.host, ntohs(sa->sin_port), strerror(errno), errno);
	} else {
		printf("SAMPLE %i %s:tcp:%i %0.3lf\n", time_s(), PREFIX, ntohs(sa->sin_port), ms / 1000.0);
	}
	close(fd);

done:
	pthread_barrier_wait(&barrier);
	pthread_exit(NULL);
}

int parse_options(int argc, char **argv)
{
	int i;
	for (i = 1; i < argc; i++) {
		if (streq(argv[i], "-H") || streq(argv[i], "--host")) {
			if (++i >= argc) {
				fprintf(stderr, "Missing required value for -H\n");
				return 1;
			}
			OPTIONS.host = strdup(argv[i]);
			continue;
		}

		if (streq(argv[i], "-p") || streq(argv[i], "--prefix")) {
			if (++i >= argc) {
				fprintf(stderr, "Missing required value for -p\n");
				return 1;
			}
			PREFIX = strdup(argv[i]);
			continue;
		}

		if (streq(argv[i], "-t") || streq(argv[i], "--timeout")) {
			if (++i >= argc) {
				fprintf(stderr, "Missing required value for -t\n");
				return 1;
			}
			OPTIONS.timeout = atoi(argv[i]);
			if (OPTIONS.timeout <= 0) OPTIONS.timeout = 2;
			continue;
		}

		if (streq(argv[i], "-h") || streq(argv[i], "-?") || streq(argv[i], "--help")) {
			fprintf(stdout, "tcp (a Bolo collector)\n"
			                "USAGE: tcp [options] port [port ...]\n"
			                "\n"
			                "options:\n"
			                "   -h, --help               Show this help screen\n"
			                "   -p, --prefix PREFIX      Use the given metric prefix\n"
			                "                            (FQDN is used by default)\n"
			                "\n"
			                "Ports must be given as unsigned, non-zero integers.\n"
			                "\n"
			                "At least one port must be specified; all will be evaluated\n"
			                "together, in sequence\n"
			                "\n");
			exit(0);
		}

		break;
	}
	if (!PREFIX) PREFIX = fqdn();
	if (!OPTIONS.host) OPTIONS.host = fqdn();
	OPTIONS.ports  = argv + i;
	OPTIONS.nports = argc - i;
	return argv[i] == NULL ? 1 : 0;
}

int resolve_ipv4(struct sockaddr_in *sa, const char *addr)
{
	sa->sin_family = AF_INET;
	int rc = inet_pton(sa->sin_family, addr, &sa->sin_addr);

	if (rc == 0) {
		// not an IP address; try DNS
		struct addrinfo hints;
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family    = AF_INET;    /* Allow IPv4 or IPv6 */
		hints.ai_socktype  = SOCK_DGRAM; /* Datagram socket */
		hints.ai_flags     = AI_PASSIVE; /* For wildcard IP address */
		hints.ai_protocol  = 0;          /* Any protocol */
		hints.ai_canonname = NULL;
		hints.ai_addr = NULL;
		hints.ai_next = NULL;

		struct addrinfo *result;
		rc = getaddrinfo(addr, NULL, &hints, &result);
		if (rc != 0) {
			fprintf(stderr, "Unable to lookup %s: %s\n", addr, gai_strerror(errno));
			exit(2);
		}

		memcpy(&sa->sin_addr, &((struct sockaddr_in*)(result->ai_addr))->sin_addr, sizeof(sa->sin_addr));

	} else if (rc != 1) {
		fprintf(stderr, "Failed to convert '%s' into a network address!\n", addr);
		exit(3);
	}

	return 0;
}
