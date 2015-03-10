#include <stdio.h>
#include <vigor.h>
#include <curl/curl.h>

#define UA PACKAGE_NAME " (nginx)/" PACKAGE_VERSION

static const char *PREFIX;
static int32_t ts;

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

int main(int argc, char **argv)
{
	if (argc > 3 || argc < 2) {
		fprintf(stderr, "USAGE: %s [prefix] URL\n", argv[0]);
		exit(1);
	}

	PREFIX = (argc == 3) ? argv[1] : fqdn();
	return collect_nginx(argc == 3 ? argv[2] : argv[1]);
}
