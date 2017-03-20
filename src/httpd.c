#include <stdio.h>
#include <vigor.h>
#include <curl/curl.h>

#include "common.h"

#define UA PACKAGE_NAME " (httpd)/" PACKAGE_VERSION

static size_t ng_writer(void *buf, size_t each, size_t n, void *user)
{
	uint64_t v[7];
	if (sscanf(buf, "Active connections: %lu\n"
	                "server accepts handled requests\n"
	                " %lu %lu %lu\n"
	                "Reading: %lu Writing: %lu Waiting: %lu\n",
	                &v[0],
	                &v[1], &v[2], &v[3],
	                &v[4], &v[5], &v[6]) == 7) {

		int32_t ts = time_s();
		printf("RATE %i %s:nginx:requests.accepted %lu\n", ts, PREFIX, v[1]);
		printf("RATE %i %s:nginx:requests.handled %lu\n",  ts, PREFIX, v[2]);
		printf("RATE %i %s:nginx:requests.total %lu\n",    ts, PREFIX, v[3]);

		printf("SAMPLE %i %s:nginx:connections.active %lu\n",  ts, PREFIX, v[0]);
		printf("SAMPLE %i %s:nginx:connections.reading %lu\n", ts, PREFIX, v[4]);
		printf("SAMPLE %i %s:nginx:connections.writing %lu\n", ts, PREFIX, v[5]);
		printf("SAMPLE %i %s:nginx:connections.waiting %lu\n", ts, PREFIX, v[6]);
	}
	return n * each;
}

static int collect_nginx(const char *url)
{
	int rc = 0;

	if (!url)
		url = "http://localhost/nginx_status";

	curl_global_init(CURL_GLOBAL_ALL);
	CURL *c = curl_easy_init();
	if (!c)
		return 1;

	char error[CURL_ERROR_SIZE];
	curl_easy_setopt(c, CURLOPT_NOSIGNAL,      1);
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, ng_writer);
	curl_easy_setopt(c, CURLOPT_USERAGENT,     UA);
	curl_easy_setopt(c, CURLOPT_ERRORBUFFER,   error);
	curl_easy_setopt(c, CURLOPT_URL,           url);

	if (curl_easy_perform(c) != CURLE_OK) {
		fprintf(stderr, "e:%s\n", error);
		return 2;
	}

	return 0;
}

static size_t apache_writer(void *buf, size_t each, size_t n, void *user)
{
	uint64_t v[4];
	double b;

	if (sscanf(buf, "Total Accesses: %lu\n"
	                "Total kBytes: %lu\n"
	                "Uptime: %*u\n"
	                "ReqPerSec: %*f\n"
	                "BytesPerSec: %*f\n"
	                "BytesPerReq: %lf\n"
	                "BusyWorkers: %lu\n"
	                "IdleWorkers: %lu\n",
	                &v[0], &v[1], &b, &v[2], &v[3]) == 5) {

		int32_t ts = time_s();
		printf("RATE %i %s:apache:requests.total %lu\n", ts, PREFIX, v[0]);
		printf("RATE %i %s:apache:requests.bytes %lu\n", ts, PREFIX, v[1] * 1024);
		printf("SAMPLE %i %s:apache:request.size %lf\n", ts, PREFIX, b);
		printf("SAMPLE %i %s:apache:workers.busy %lu\n", ts, PREFIX, v[2]);
		printf("SAMPLE %i %s:apache:workers.idle %lu\n", ts, PREFIX, v[3]);
	} else
	if (sscanf(buf, "Total Accesses: %lu\n"
	                "Total kBytes: %lu\n"
	                "CPULoad: %*f\n"
	                "Uptime: %*u\n"
	                "ReqPerSec: %*f\n"
	                "BytesPerSec: %*f\n"
	                "BytesPerReq: %lf\n"
	                "BusyWorkers: %lu\n"
	                "IdleWorkers: %lu\n",
	                &v[0], &v[1], &b, &v[2], &v[3]) == 5) {

		int32_t ts = time_s();
		printf("RATE %i %s:apache:requests.total %lu\n", ts, PREFIX, v[0]);
		printf("RATE %i %s:apache:requests.bytes %lu\n", ts, PREFIX, v[1] * 1024);
		printf("SAMPLE %i %s:apache:request.size %lf\n", ts, PREFIX, b);
		printf("SAMPLE %i %s:apache:workers.busy %lu\n", ts, PREFIX, v[2]);
		printf("SAMPLE %i %s:apache:workers.idle %lu\n", ts, PREFIX, v[3]);
	}
	return n * each;
}

static int collect_apache(const char *url)
{
	int rc = 0;

	if (!url)
		url = "http://localhost/server-status?auto";

	curl_global_init(CURL_GLOBAL_ALL);
	CURL *c = curl_easy_init();
	if (!c)
		return 1;

	char error[CURL_ERROR_SIZE];
	curl_easy_setopt(c, CURLOPT_NOSIGNAL,      1);
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, apache_writer);
	curl_easy_setopt(c, CURLOPT_USERAGENT,     UA);
	curl_easy_setopt(c, CURLOPT_ERRORBUFFER,   error);
	curl_easy_setopt(c, CURLOPT_URL,           url);

	if (curl_easy_perform(c) != CURLE_OK) {
		fprintf(stderr, "e:%s\n", error);
		return 2;
	}

	return 0;
}

static char *URL = NULL;
static int (*COLLECTOR)(const char *);
int parse_options(int argc, char **argv);

int main(int argc, char **argv)
{
	if (parse_options(argc, argv) != 0) {
		fprintf(stderr, "USAGE: %s [options] URL\n", argv[0]);
		exit(1);
	}

	return COLLECTOR(URL);
}

int parse_options(int argc, char **argv)
{
	COLLECTOR = collect_nginx; /* default */
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

		if (streq(argv[i], "-t") || streq(argv[i], "--type")) {
			if (++i >= argc) {
				fprintf(stderr, "Missing required value for -t\n");
				return 1;
			}
			if (streq(argv[i], "nginx")) {
				COLLECTOR = collect_nginx;
				continue;
			}
			if (streq(argv[i], "apache")) {
				COLLECTOR = collect_apache;
				continue;
			}
			fprintf(stderr, "Unrecognized HTTP server type '%s'\n", argv[i]);
			return 1;
		}

		if (streq(argv[i], "-h") || streq(argv[i], "-?") || streq(argv[i], "--help")) {
			fprintf(stdout, "httpd (a Bolo collector)\n"
			                "USAGE: httpd [options] URL\n"
			                "\n"
			                "options:\n"
			                "   -h, --help               Show this help screen\n"
			                "   -p, --prefix PREFIX      Use the given metric prefix\n"
			                "                            (FQDN is used by default)\n"
			                "   -t, --type TYPE          What type of HTTP server.  Default to 'nginx'\n"
			                "                            Valid values: apache, nginx\n"
			                "\n");
			exit(0);
		}

		if (!URL && argv[i][0] != '-') {
			URL = strdup(argv[i]);

		} else {
			fprintf(stderr, "Unrecognized argument '%s'\n", argv[i]);
			errors++;
		}
	}

	INIT_PREFIX();

	return errors;
}
