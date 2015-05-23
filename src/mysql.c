#include "common.h"
#define list_delete mysql_list_delete
#include <mysql/mysql.h>
#undef list_delete
#include <dlfcn.h>

void *libmysqlclient;
MYSQL *((*_mysql_init)(MYSQL*));
void   ((*_mysql_close)(MYSQL*));
MYSQL *((*_mysql_real_connect)(MYSQL*, const char*, const char*, const char*, const char*,
                               unsigned int, const char*, unsigned long));
const char *((*_mysql_error)(MYSQL*));
int ((*_mysql_query)(MYSQL*, const char*));
MYSQL_RES* ((*_mysql_store_result)(MYSQL*));
MYSQL_ROW* ((*_mysql_fetch_row)(MYSQL_RES*));
MYSQL_FIELD* ((*_mysql_fetch_field)(MYSQL_RES*));

static int skip_empty(const char *s, const char *name)
{
	if (s && *s) return 0;
	fprintf(stderr, "skipping result with empty '%s' field\n", name);
	return 1;
}

static void run_query(MYSQL *db, const char *sql)
{
	ts = time_s();
	if (_mysql_query(db, sql) != 0) {
		fprintf(stderr, "`%s' failed\nerror: %s", sql, _mysql_error(db));
		return;
	}

	MYSQL_RES *res = _mysql_store_result(db);
	if (!res) {
		fprintf(stderr, "`%s' failed\nerror: %s", sql, _mysql_error(db));
		return;
	}

	int i = 0;
	int tcol = -1, vcol = -1, ncol = -1;
	MYSQL_FIELD *field;
	while ((field = _mysql_fetch_field(res)) != NULL) {
		if (strcmp(field->name, "type")  == 0) tcol = i;
		if (strcmp(field->name, "value") == 0) vcol = i;
		if (strcmp(field->name, "name")  == 0) ncol = i;
		i++;
	}
	if (tcol < 0) fprintf(stderr, "missing 'type' field in SQL query\n");
	if (vcol < 0) fprintf(stderr, "missing 'value' field in SQL query\n");
	if (ncol < 0) fprintf(stderr, "missing 'name' field in SQL query\n");
	if (tcol < 0 || vcol < 0 || ncol < 0)
		return;

	MYSQL_ROW *row;
	while ((row = _mysql_fetch_row(res)) != NULL) {
		if (skip_empty((const char*)row[tcol], "type")) continue;
		if (skip_empty((const char*)row[vcol], "value")) continue;
		printf("%s %i %s:mysql:%s %s\n", row[tcol], ts, PREFIX,
			(row[ncol] && *row[ncol]) ? (const char*)row[ncol] : "unnamed", row[vcol]);
	}
}

#define BUF_SIZE 16384
static void run_queries(MYSQL *db, FILE *io)
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

static int read_creds(const char *file, char **user, char **pass)
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
	char *database = strdup("mysql");
	char *host     = strdup("localhost");
	char *port     = strdup("3306");

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
			fprintf(stdout, "mysql (a Bolo collector)\n"
			                "USAGE: mysql [options] /path/to/queries.sql\n"
			                "\n"
			                "options:\n"
			                "   -h, --help               Show this help screen\n"
			                "   -p, --prefix PREFIX      Use the given metric prefix\n"
			                "                            (FQDN is used by default)\n"
			                "   -c, --credentials FILE   Path to a file with username / password\n"
			                "   -d, --database NAME      Database to connect to\n"
			                "                            (Defaults to 'postgres')\n"
			                "   -H, --host ADDRESS       Hostname or IP to connect to\n"
			                "                            (Defaults to localhost)\n"
			                "   -P, --port NUMBER        TCP port to connect to\n"
			                "                            (Defaults to 5432)\n"
			                "\n");
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
		fprintf(stderr, "USAGE: %s [options] /path/to/queries.sql\n", argv[0]);
		return 1;
	}

	char *user = NULL;
	char *pass = NULL;
	if (creds && read_creds(creds, &user, &pass) != 0)
		exit(1);
	if (!user)
		user = strdup("root");

	libmysqlclient = dlopen("libmysqlclient.so", RTLD_LAZY|RTLD_DEEPBIND);
	if (!libmysqlclient) {
		fprintf(stderr, "failed to dlopen libmysqlclient.so: %s\n", dlerror());
		return 1;
	}

	_mysql_init         = dlsym(libmysqlclient, "mysql_init");
	_mysql_close        = dlsym(libmysqlclient, "mysql_close");
	_mysql_query        = dlsym(libmysqlclient, "mysql_query");
	_mysql_error        = dlsym(libmysqlclient, "mysql_error");
	_mysql_real_connect = dlsym(libmysqlclient, "mysql_real_connect");
	_mysql_store_result = dlsym(libmysqlclient, "mysql_store_result");
	_mysql_fetch_row    = dlsym(libmysqlclient, "mysql_fetch_row");
	_mysql_fetch_field  = dlsym(libmysqlclient, "mysql_fetch_field");

	MYSQL *db = _mysql_init(NULL);
	if (!db) {
		fprintf(stderr, "initialization failed: %s\n", _mysql_error(db));
		return 1;
	}
	if (!_mysql_real_connect(db, host, user, pass, database, atoi(port), NULL, 0)) {
		fprintf(stderr, "connection failed: %s\n", _mysql_error(db));
		_mysql_close(db);
		return 1;
	}

	FILE *io = stdin;
	if (!streq(argv[optind], "-")) {
		io = fopen(argv[optind], "r");
		if (!io) perror(argv[optind]);
	}
	run_queries(db, io);
	fclose(io);
	_mysql_close(db);
	return 0;
}
