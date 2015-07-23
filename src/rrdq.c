#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>

#define HAVE_STDINT_H
#include <rrd.h>
//#include <rrd_client.h>


typedef struct {
	int    skip_unknown;
	double unknown;
	float  percentile;
} cf_arg_t;
typedef double (*cf_fn)(size_t, double*, cf_arg_t*);

struct {
	int   DEBUG;
	char *root;
	char *hash;

	char *metric;
	char *ds;
	char *rrdfile;

	time_t start;
	time_t end;

	cf_fn    cf;
	cf_arg_t cf_arg;
} OPTIONS = { 0 };

double cf_min      (size_t, double*, cf_arg_t*);
double cf_max      (size_t, double*, cf_arg_t*);
double cf_sum      (size_t, double*, cf_arg_t*);
double cf_mean     (size_t, double*, cf_arg_t*);
double cf_median   (size_t, double*, cf_arg_t*);
double cf_stddev   (size_t, double*, cf_arg_t*);
double cf_variance (size_t, double*, cf_arg_t*);
double cf_nth      (size_t, double*, cf_arg_t*);

int parse_options(int argc, char **argv);

int main(int argc, char **argv)
{
	if (parse_options(argc, argv) != 0) {
		fprintf(stderr, "USAGE: %s -t start:end <cf> <metric:name:with:ds>\n", argv[0]);
		exit(1);
	}
	if (OPTIONS.DEBUG) {
		fprintf(stderr, " cf      min = %p\n", cf_min);
		fprintf(stderr, " cf      max = %p\n", cf_max);
		fprintf(stderr, " cf      sum = %p\n", cf_sum);
		fprintf(stderr, " cf     mean = %p\n", cf_mean);
		fprintf(stderr, " cf   median = %p\n", cf_median);
		fprintf(stderr, " cf   stddev = %p\n", cf_stddev);
		fprintf(stderr, " cf variance = %p\n", cf_variance);
		fprintf(stderr, " cf      nth = %p\n", cf_nth);

		fprintf(stderr, "-------------------------\n\n\n");
		fprintf(stderr, "metric = %s\n", OPTIONS.metric);
		fprintf(stderr, "    ds = %s\n", OPTIONS.ds);
		fprintf(stderr, "  file = %s\n", OPTIONS.rrdfile);
		fprintf(stderr, "  root = %s\n", OPTIONS.root);
		fprintf(stderr, "  hash = %s\n", OPTIONS.hash);
		fprintf(stderr, " start = %lu\n", OPTIONS.start);
		fprintf(stderr, "   end = %lu\n", OPTIONS.end);
		fprintf(stderr, "    cf = %p\n", OPTIONS.cf);
		if (OPTIONS.cf == cf_nth)
			fprintf(stderr, "     p = %f\n", OPTIONS.cf_arg.percentile);
		if (OPTIONS.cf_arg.skip_unknown)
			fprintf(stderr, "     U = ignore/skip\n");
		else
			fprintf(stderr, "     U = %f\n", OPTIONS.cf_arg.unknown);
		fprintf(stderr, "\n\n");
	}

	char          **ds_names;
	unsigned long   ds_count;
	unsigned long   step = 1;
	rrd_value_t *raw;
	int rc;

	rc = rrd_fetch_r(OPTIONS.rrdfile, "AVERAGE", &OPTIONS.start, &OPTIONS.end, &step,
			&ds_count, &ds_names, &raw);
	if (rc != 0) {
		fprintf(stderr, "fetch failed!\n");
		if (rrd_test_error()) {
			fprintf(stderr, "rrd said: %s\n", rrd_get_error());
		}
		exit(2);
	}

	int ds;
	for (ds = 0; ds < ds_count; ds++)
		if (strcmp(OPTIONS.ds, ds_names[ds]) == 0)
			break;
	if (ds >= ds_count) {
		fprintf(stderr, "DS '%s' not found in RRD file\n", OPTIONS.ds);
		exit(2);
	}

	size_t n = (OPTIONS.end - OPTIONS.start) / step;
	double *set = calloc(n, sizeof(double));
	rrd_value_t *d = raw + ds;
	int i, j;
	for (i = 0, j = 0; i < n; i++) {
		double v = (double)*d;
		if (isnan(v)) {
			if (OPTIONS.cf_arg.skip_unknown) {
				if (OPTIONS.DEBUG)
					fprintf(stderr, "skipping sample #%i (--unknown=ignore)\n", i+1);
				continue;
			}

			if (OPTIONS.DEBUG)
				fprintf(stderr, "sample #%i is UNKNOWN (substituting %e)\n", i+1, OPTIONS.cf_arg.unknown);
			v = OPTIONS.cf_arg.unknown;
		}

		set[j] = v;
		if (OPTIONS.DEBUG)
			fprintf(stderr, "[%i] %e (%lf)\n", j+1, set[j], set[j]);
		d += ds_count;
		j++;
	}
	printf("%e\n", (*OPTIONS.cf)(j, set, &OPTIONS.cf_arg));

	return 0;
}

int parse_options(int argc, char **argv)
{
	OPTIONS.root = strdup("/var/lib/bolo/rrd");
	OPTIONS.cf_arg.skip_unknown = 1;

	int i;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-?") == 0 || strcmp(argv[i], "--help") == 0) {
			fprintf(stderr, "rrdq (a Bolo utility)\n"
			                "USAGE: rrdq -t start:end <cf> <metric:name:with:ds>\n"
			                "\n"
			                "Queries a bolo RRD (as created by bolo2rrd) and calculates a single,\n"
			                "aggregate value for a given time frame, using one of several methods\n"
			                "of consolidation (min, max, standard deviation, etc.)\n"
			                "\n"
			                "\n"
			                "options:\n"
			                "   -h, --help               Show this help screen\n"
			                "   -t, --time start:end     Specify the time range for analysis\n"
			                "   --hash /path/to/map      Path to the bolo2rrd hash map file\n"
			                "                            (if you don't know what that is, you\n"
			                "                             probably don't need it)\n"
			                "   --root /path/to/rrds     Root directory where RRD files are stored.\n"
			                "                            (defaults to /var/lib/bolo/rrd)\n"
			                "  -u, --unknown <value>     Treat unknown (U) samples as if they were\n"
			                "                            this value instead.  The special value\n"
			                "                            'ignore' (the default) will cause such\n"
			                "                            samples to be removed from the set before\n"
			                "                            consolidation.\n"
			                "\n"
			                "\n"
			                "The acceptable values for <cf>, the consolidation function, are:\n"
			                "\n"
			                "           min  Smallest value in the sample set.\n"
			                "           max  Largest value in the sample set.\n"
			                "           sum  Summation of all values in the sample set.\n"
			                "          mean  The arithmetic mean (sum / count).\n"
			                "        median  The exact midpoint of the set's range (50th percentile).\n"
			                "        stddev  Standard deviation of the sample set.\n"
			                "      variance  Variance of the sample set (standard deviation squared).\n"
			                "         <N>th  The N-th percentile, the value at which the set is\n"
			                "                divided into two subsets, one containing N%% of set\n"
			                "                and the other containing 100-N%% (the remainder).\n"
			                "                <N> can be specified as a whole number (50, 75, etc.)\n"
			                "                or a decimal value (99.999, 0.001, etc.)\n"
			                "\n");
			exit(0);
		}

		if (strcmp(argv[i], "-D") == 0 || strcmp(argv[i], "--debug") == 0) {
			OPTIONS.DEBUG = 1;
			continue;
		}

		if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--time") == 0) {
			if (++i >= argc) {
				fprintf(stderr, "Missing required value for -t\n");
				return 1;
			}

			struct {
				signed int a_quant;
				char       a_unit;
				signed int b_quant;
				char       b_unit;
			} spec;
			if (sscanf(argv[i], "%d%c:%d%c",
			                    &spec.a_quant, &spec.a_unit,
			                    &spec.b_quant, &spec.b_unit) != 4) {
				fprintf(stderr, "Bad value '%s' for --time\n", argv[i]);
				return 1;
			}

			switch (spec.a_unit) {
			case 'd': spec.a_quant *= 24;
			case 'h': spec.a_quant *= 60;
			case 'm': spec.a_quant *= 60;
			case 's': break;
			default:
				fprintf(stderr, "Bad unit for start of window ('%c')\n"
				                "Must be one of d (days), h (hours), m (minutes) or s (seconds)\n",
				                spec.a_unit);
				return 1;
			}

			switch (spec.b_unit) {
			case 'd': spec.b_quant *= 24;
			case 'h': spec.b_quant *= 60;
			case 'm': spec.b_quant *= 60;
			case 's': break;
			default:
				fprintf(stderr, "Bad unit for window duration ('%c')\n"
				                "Must be one of d (days), h (hours), m (minutes) or s (seconds)\n",
				                spec.b_unit);
				return 1;
			}

			if (spec.a_quant > 0)
				spec.a_quant *= -1;

			time_t now = time(NULL);
			OPTIONS.start = now + spec.a_quant;
			if (spec.b_quant > 0) {
				OPTIONS.end = OPTIONS.start + spec.b_quant;
			} else {
				OPTIONS.end = now + spec.b_quant;
			}

			if (OPTIONS.end <= OPTIONS.start) {
				fprintf(stderr, "Invalid window (starts after it ends)\n");
				return 1;
			}

			continue;
		}

		if (strcmp(argv[i], "--hash") == 0) {
			if (++i >= argc) {
				fprintf(stderr, "Missing required value for --hash\n");
				return 1;
			}

			free(OPTIONS.hash);
			OPTIONS.hash = strdup(argv[i]);
			continue;
		}

		if (strcmp(argv[i], "--root") == 0) {
			if (++i >= argc) {
				fprintf(stderr, "Missing required value for --root\n");
				return 1;
			}

			free(OPTIONS.root);
			OPTIONS.root = strdup(argv[i]);
			continue;
		}

		if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--unknown") == 0) {
			if (++i >= argc) {
				fprintf(stderr, "Missing required value for --unknown\n");
				return 1;
			}

			if (strcmp(argv[i], "ignore") == 0) {
				OPTIONS.cf_arg.skip_unknown = 1;
			} else {
				OPTIONS.cf_arg.skip_unknown = 0;
				if (sscanf(argv[i], "%lf", &OPTIONS.cf_arg.unknown) != 1) {
					fprintf(stderr, "Bad value '%s' for --unknown\n", argv[i]);
					return 1;
				}
			}
			continue;
		}

		if (strcmp(argv[i], "min") == 0) {
			OPTIONS.cf = cf_min;
			continue;
		}

		if (strcmp(argv[i], "max") == 0) {
			OPTIONS.cf = cf_max;
			continue;
		}

		if (strcmp(argv[i], "sum") == 0) {
			OPTIONS.cf = cf_sum;
			continue;
		}

		if (strcmp(argv[i], "mean") == 0) {
			OPTIONS.cf = cf_mean;
			continue;
		}

		if (strcmp(argv[i], "median") == 0) {
			OPTIONS.cf = cf_median;
			continue;
		}

		if (strcmp(argv[i], "stddev") == 0) {
			OPTIONS.cf = cf_stddev;
			continue;
		}

		if (strcmp(argv[i], "variance") == 0) {
			OPTIONS.cf = cf_variance;
			continue;
		}

		float p;
		if (sscanf(argv[i], "%fth", &p) == 1
		 || sscanf(argv[i], "%fnd", &p) == 1
		 || sscanf(argv[i], "%frd", &p) == 1
		 || sscanf(argv[i], "%fst", &p) == 1) {
			OPTIONS.cf = cf_nth;
			OPTIONS.cf_arg.percentile = p / 100.0;
			continue;
		}

		char *delim = strrchr(argv[i], ':');
		if (delim) {
			free(OPTIONS.metric);
			OPTIONS.metric = calloc(1, delim - argv[i] + 1);
			memcpy(OPTIONS.metric, argv[i], delim - argv[i]);

			free(OPTIONS.ds);
			OPTIONS.ds = strdup(delim + 1);
			continue;
		}

		fprintf(stderr, "Unrecognized argument '%s'\n", argv[i]);
		return 1;
	}

	if (!OPTIONS.metric || !OPTIONS.ds) {
		fprintf(stderr, "Missing metric:ds argument!\n");
		return 1;
	}
	if (!OPTIONS.cf) {
		fprintf(stderr, "No consolidation function provided\n");
		return 1;
	}

	if (OPTIONS.hash) {
		FILE *io = fopen(OPTIONS.hash, "r");
		if (!io) {
			fprintf(stderr, "%s: %s\n", OPTIONS.hash, strerror(errno));
			return 1;
		}

		char buf[8192];
		while (fgets(buf, 8192, io)) {
			char *m = strchr(buf, ' ');
			if (!m) continue;
			*m++ = '\0';

			if (strcmp(m, OPTIONS.metric) == 0) {
				if (asprintf(&OPTIONS.rrdfile, "%s/%s.rrd", OPTIONS.root, buf) <= 0) {
					perror("asprintf");
					exit(9);
				}
				break;
			}
		}

		fclose(io);

	} else {
		if (asprintf(&OPTIONS.rrdfile, "%s/%s", OPTIONS.root, OPTIONS.metric) <= 0) {
			perror("asprintf");
			exit(9);
		}
	}

	return 0;
}

double cf_min(size_t n, double *set, cf_arg_t *arg)
{
	if (n == 0) return NAN;
	double x = set[0];
	int i;
	for (i = 1; i < n; i++)
		if (set[i] < x)
			x = set[i];
	return x;
}

double cf_max(size_t n, double *set, cf_arg_t *arg)
{
	if (n == 0) return NAN;
	double x = set[0];
	int i;
	for (i = 1; i < n; i++)
		if (set[i] > x)
			x = set[i];
	return x;
}

double cf_sum(size_t n, double *set, cf_arg_t *arg)
{
	double x = 0.0;
	int i;
	for (i = 0; i < n; i++)
		x += set[i];
	return x;
}

double cf_mean(size_t n, double *set, cf_arg_t *arg)
{
	double x = 0.0;
	int i;
	for (i = 0; i < n; i++)
		x += set[i] / n;
	return x;
}

static int cmpd(const void *a, const void *b)
{
	return *(double * const)a - *(double * const)b;
}
double cf_median(size_t n, double *set, cf_arg_t *arg)
{
	arg->percentile = 0.5;
	return cf_nth(n, set, arg);

	qsort(set, n, sizeof(double), cmpd);
	size_t mid = n / 2;
	if (n == 0)     return NAN;
	if (n % 2 == 1) return set[mid];
	else            return (set[mid] + set[mid + 1]) / 2;
}

double cf_stddev(size_t n, double *set, cf_arg_t *arg)
{
	if (n == 0) return NAN;
	return sqrt(cf_variance(n, set, arg));
}

double cf_variance(size_t n, double *set, cf_arg_t *arg)
{
	if (n == 0) return NAN;
	double mean = cf_mean(n, set, arg);
	double x = 0.0;
	int i;
	for (i = 0; i < n; i++)
		x += ((set[i] - mean) * (set[i] - mean)) / n;
	return x;
}

double cf_nth(size_t n, double *set, cf_arg_t *arg)
{
	qsort(set, n, sizeof(double), cmpd);
	double mid = n * arg->percentile;
	if (fabs(floor(mid) - mid) < 0.001)
		return (set[(int)(mid)] + set[(int)(mid + 1)]) / 2;
	return set[(int)(mid)];
}
