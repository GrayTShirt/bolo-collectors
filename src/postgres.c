#include <assert.h>
#include <stdio.h>
#include <postgresql/libpq-fe.h>
#include <getopt.h>
#include <string.h>
#include <vigor.h>

static char *PREFIX;
static int32_t ts;

static int column(PGresult *r, const char *name) {
	int c = PQfnumber(r, name);
	if (c < 0) fprintf(stderr, "missing '%s' field in SQL query\n", name);
	return c;
}

static int skip_empty(const char *s, const char *name)
{
	if (s && *s) return 0;
	fprintf(stderr, "skipping result with empty '%s' field\n", name);
	return 1;
}

static void run_query(PGconn *db, const char *sql)
{
	ts = time_s();
	PGresult *r = PQexec(db, sql);
	if (PQresultStatus(r) != PGRES_TUPLES_OK) {
		fprintf(stderr, "`%s' failed\nerror: %s", sql, PQresultErrorMessage(r));
		return;
	}

	int tcol = column(r, "type");
	int vcol = column(r, "value");
	int ncol = column(r, "name");
	if (tcol < 0 || vcol < 0 || ncol < 0)
		return;

	int i, count = PQntuples(r);
	for (i = 0; i < count; i++) {
		char *type  = PQgetvalue(r, i, tcol);
		char *value = PQgetvalue(r, i, vcol);
		char *name  = PQgetvalue(r, i, ncol);

		if (skip_empty(type,  "type"))  continue;
		if (skip_empty(value, "value")) continue;
		printf("%s %i %s:postgres:%s %s\n", type, ts, PREFIX,
			(name && *name) ? name : "unnamed", value);
	}
	PQclear(r);
}

#define BUF_SIZE 16384
void run_queries(PGconn *db, FILE *io)
{
	char *buf = vmalloc(BUF_SIZE);
	while (fgets(buf, BUF_SIZE, io) != NULL) {
		char *sql = buf;
		while (isspace(*sql)) sql++;

		if (!*sql || *sql == '-' || *sql == '#' || *sql == ';')
			continue; /* blank line or comment */

		run_query(db, sql);
	}
}

int read_creds(const char *file, char **user, char **pass)
{
	FILE *io = fopen(file, "r");
	if (!io) {
		perror(file);
		return 1;
	}

	char buf[256];
	if (fgets(buf, 256, io) == NULL) {
		fprintf(stderr, "no credentials found");
		return 1;
	}

	char *sep = strchr(buf, ':');
	char *end = strrchr(buf, '\n');

	if (!sep || !end) {
		fprintf(stderr, "no credentials found");
		return 1;
	}

	*end = '\0';
	*sep++ = '\0';

	*user = strdup(buf);
	*pass = strdup(sep);
	return 0;
}

int main(int argc, char **argv)
{
	PREFIX = fqdn();

	char *creds    = NULL;
	char *database = strdup("postgres");
	char *host     = strdup("localhost");
	char *port     = strdup("5432");

	const char *short_opts = "h?p:c:d:H:P:";
	struct option long_opts[] = {
		{ "help",              no_argument, 0, 'h' },
		{ "prefix",      required_argument, 0, 'p' },
		{ "credentials", required_argument, 0, 'c' },
		{ "creds",       required_argument, 0, 'c' },
		{ "database",    required_argument, 0, 'd' },
		{ "host",        required_argument, 0, 'H' },
		{ "port",        required_argument, 0, 'P' },
		{ 0, 0, 0, 0 },
	};
	int opt, idx;
	while ((opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) >= 0) {
		switch (opt) {
		case 'h':
		case '?':
			fprintf(stderr, "USAGE: %s [OPTIONS] /path/to/queries.sql\n", argv[0]);
			exit(0);

		case 'p':
			free(PREFIX);
			PREFIX = strdup(optarg);
			break;

		case 'c':
			free(creds);
			creds = strdup(optarg);
			break;

		case 'H':
			free(host);
			host = strdup(optarg);
			break;

		case 'P':
			free(port);
			port = strdup(optarg);
			break;

		case 'd':
			free(database);
			database = strdup(optarg);
			break;
		}
	}

	if (!argv[optind]) {
		fprintf(stderr, "USAGE: %s [OPTIONS] /path/to/queries.sql\n", argv[0]);
		return 1;
	}

	char *user = NULL;
	char *pass = NULL;
	if (creds && read_creds(creds, &user, &pass) != 0)
		exit(1);

	char *dsn;
	if (user && pass)
		dsn = string("host=%s port=%s dbname=%s user=%s password=%s",
			host, port, database, user, pass);
	else
		dsn = string("host=%s port=%s dbname=%s",
			host, port, database);

	PGconn *db = PQconnectdb(dsn);
	assert(db);
	if (PQstatus(db) != CONNECTION_OK) {
		fprintf(stderr, "connection failed\n");
		return 1;
	}

	FILE *io = stdin;
	if (strcmp(argv[optind], "-") != 0) {
		io = fopen(argv[optind], "r");
		if (!io) perror(argv[optind]);
	}
	run_queries(db, io);
	fclose(io);
	return 0;
}
