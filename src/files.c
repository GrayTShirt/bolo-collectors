#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <fnmatch.h>
#include <vigor.h>

static char *PREFIX;
static int32_t ts;

#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define MAX(a,b) ((a) < (b) ? (b) : (a))

#define OP_NOT  3
#define OP_AND  2
#define OP_OR   1
#define OP_NONE 0

static const char *OP_NAMES[] = { "INVALID", "-or", "-and", "-not" };

typedef enum {
	/* DAYSTART */
	PR_MAXDEPTH = 1,
	PR_MINDEPTH,
	/* XDEV */
	PR_AMIN, PR_ATIME, PR_ANEWER,
	PR_CMIN, PR_CTIME, PR_CNEWER,
	PR_MMIN, PR_MTIME, PR_MNEWER,
	PR_EMPTY,
	PR_TRUE, PR_FALSE,
	PR_TYPE, PR_XTYPE, /* FSTYPE */
	PR_GID, PR_GROUP, /* NOGROUP */
	PR_UID, PR_USER, /* NOUSER */
	PR_LNAME, PR_ILNAME, PR_NAME, PR_INAME, PR_PATH, PR_IPATH,
	/* REGEX, IREGEX */
	PR_INUM, PR_LINKS,
	/* PERM */
	PR_READABLE, PR_WRITABLE,
	PR_SAMEFILE,
	PR_SIZE,
} pr_t;

static const char *PR_TYPE_NAMES[] = {
	"UNKNOWN",
	"-maxdepth",
	"-mindepth",
	"-amin", "-atime", "-anewer",
	"-cmin", "-ctime", "-cnewer",
	"-mmin", "-mtime", "-mnewer",
	"-empty",
	"-true", "-false",
	"-type", "-xtype",
	"-gid", "-group",
	"-uid", "-user",
	"-lname", "-ilname", "-name", "-iname", "-path", "-ipath",
	"-inum", "-links",
	"-readable", "-writable",
	"-samefile",
	"-size",
};

#define MATCH_GT  1
#define MATCH_EQ  0
#define MATCH_LT -1

#define AGGREGATE_SUM 1
#define AGGREGATE_MIN 2
#define AGGREGATE_MAX 3
#define AGGREGATE_AVG 4

#define TRACK_SIZE    1
#define TRACK_COUNT   2

typedef struct {
	char *path;
	int   time_start;
	int   xdev;
	int   aggregate;
	int   track;
	int   debug;
	int   dumptree;

	uint64_t count;
	struct {
		uint64_t min;
		uint64_t max;
		uint64_t sum;
	} size;
} context_t;

typedef struct {
	pr_t type;
	int match;
	union {
		char       *string;
		int64_t     i64;
		struct stat stat;
	} arg;
} pred_t;

struct _expr_t;
typedef struct _expr_t expr_t;
struct _expr_t {
	pred_t *pr;
	int     op;

	expr_t *L;
	expr_t *R;
};

#define PARSER_STACK_MAX 1024
#define PARSER_RPN_MAX   1024
typedef struct {
	int  i;
	int  argc;
	char **argv;
	int  debug;

	expr_t *rpn[PARSER_RPN_MAX];
	int     len;

	int     ops[PARSER_STACK_MAX];
	int     top;

	expr_t *last;
} parser_t;

static int compare(pred_t *p, int64_t a, int64_t b)
{
	return (a  < b && p->match == MATCH_LT)
	    || (a  > b && p->match == MATCH_GT)
	    || (a == b && p->match == MATCH_EQ);
}

static expr_t *make_oper(int op)
{
	expr_t *e = calloc(1, sizeof(expr_t));
	e->op = op;
	return e;
}

static void predicate_numeric(pred_t *p, const char *arg, int scale)
{
	p->arg.i64 = strtoll(arg, NULL, 0) * scale;
	switch (arg[0]) {
	case '+': p->match = MATCH_GT; break;
	case '-': p->match = MATCH_LT; break;
	default:  p->match = MATCH_EQ; break;
	}
}

static expr_t *make_predicate(pr_t type, const char *arg)
{
	expr_t *e = make_oper(OP_NONE);
	e->pr = calloc(1, sizeof(pred_t));
	e->pr->type = type;

	switch (type) {
	case PR_EMPTY:
	case PR_TRUE:
	case PR_FALSE:
	case PR_READABLE:
	case PR_WRITABLE:
		/* no argument */
		break;

	case PR_TYPE:
	case PR_XTYPE:
	case PR_USER:
	case PR_GROUP:
	case PR_LNAME:
	case PR_ILNAME:
	case PR_NAME:
	case PR_INAME:
	case PR_PATH:
	case PR_IPATH:
		/* simple string argument */
		e->pr->arg.string = strdup(arg);
		break;

	case PR_MAXDEPTH:
	case PR_MINDEPTH:
	case PR_LINKS:
	case PR_SIZE:
		/* numeric argument */
		predicate_numeric(e->pr, arg, 1);
		break;

	case PR_UID:
	case PR_GID:
	case PR_INUM:
		/* numeric argument */
		e->pr->arg.i64 = strtoll(arg, NULL, 0);
		break;

	case PR_ATIME:
	case PR_MTIME:
	case PR_CTIME:
		/* time argument */
		predicate_numeric(e->pr, arg, 86400);
		break;

	case PR_AMIN:
	case PR_MMIN:
	case PR_CMIN:
		/* time argument */
		predicate_numeric(e->pr, arg, 60);
		break;

	case PR_ANEWER:
	case PR_MNEWER:
	case PR_CNEWER:
	case PR_SAMEFILE:
		/* file reference argument */
		lstat(arg, &e->pr->arg.stat);
		break;
	}

	return e;
}

static void s_push_rpn(parser_t *p, expr_t *e)
{
	if (p->len == PARSER_RPN_MAX) {
		fprintf(stderr, "rpn overflow!\n");
		exit(4);
	}
	p->rpn[p->len++] = e;
}

static expr_t* s_pop_rpn(parser_t *p)
{
	if (p->len == 0) {
		fprintf(stderr, "rpn underflow!\n");
		exit(4);
	}
	return p->rpn[--p->len];
}

static void s_push_op(parser_t *p, int op)
{
	if (p->top == PARSER_STACK_MAX) {
		fprintf(stderr, "operator stack overflow!\n");
		exit(4);
	}

	while (p->top > 0 && op <= p->ops[p->top - 1])
		s_push_rpn(p, make_oper(p->ops[--p->top]));

	p->last = NULL;
	p->ops[p->top++] = op;
}

static void s_push_pr(parser_t *p, pr_t type, const char *v)
{
	if (p->last) s_push_op(p, OP_AND);
	s_push_rpn(p, p->last = make_predicate(type, v));
}

static expr_t* s_resolve(parser_t *p)
{
	expr_t *e = s_pop_rpn(p);
	switch (e->op) {
	case OP_AND:
	case OP_OR:  e->R = s_resolve(p);
	case OP_NOT: e->L = s_resolve(p);
	             break;
	}
	return e;
}

#define OPER(p,a,s,op)   if (strcmp((a),(s)) == 0) { s_push_op((p), OP_ ## op);      (p)->i++; continue; }
#define PRED(p,a,s,pr,v) if (strcmp((a),(s)) == 0) { s_push_pr((p), PR_ ## pr, (v)); (p)->i++; continue; }
static expr_t* s_parse(parser_t *p)
{
	while (p->i < p->argc) {
		char *a, *v;

		a = p->argv[p->i];
		OPER(p, a, "-and", AND); OPER(p, a, "-a", AND);
		OPER(p, a, "-or",  OR);  OPER(p, a, "-o", OR);
		OPER(p, a, "-not", NOT); OPER(p, a, "!",  NOT);

		PRED(p, a, "-empty",    EMPTY,    NULL);
		PRED(p, a, "-true",     TRUE,     NULL);
		PRED(p, a, "-false",    FALSE,    NULL);
		PRED(p, a, "-readable", READABLE, NULL);
		PRED(p, a, "-writable", WRITABLE, NULL);

		p->i++;
		if (p->i >= p->argc) {
			fprintf(stderr, "%s requires a value\n", a);
			exit(1);
		}

		v = p->argv[p->i];
		PRED(p, a, "-maxdepth", MAXDEPTH, v);
		PRED(p, a, "-mindepth", MINDEPTH, v);
		PRED(p, a, "-amin",     AMIN,     v);
		PRED(p, a, "-atime",    ATIME,    v);
		PRED(p, a, "-anewer",   ANEWER,   v);
		PRED(p, a, "-mmin",     MMIN,     v);
		PRED(p, a, "-mtime",    MTIME,    v);
		PRED(p, a, "-mnewer",   MNEWER,   v);
		PRED(p, a, "-cmin",     CMIN,     v);
		PRED(p, a, "-ctime",    CTIME,    v);
		PRED(p, a, "-cnewer",   CNEWER,   v);
		PRED(p, a, "-type",     TYPE,     v);
		PRED(p, a, "-xtype",    XTYPE,    v);
		PRED(p, a, "-gid",      GID,      v);
		PRED(p, a, "-group",    GROUP,    v);
		PRED(p, a, "-uid",      UID,      v);
		PRED(p, a, "-user",     USER,     v);
		PRED(p, a, "-lname",    LNAME,    v);
		PRED(p, a, "-ilname",   ILNAME,   v);
		PRED(p, a, "-name",     NAME,     v);
		PRED(p, a, "-iname",    INAME,    v);
		PRED(p, a, "-path",     PATH,     v);
		PRED(p, a, "-ipath",    IPATH,    v);
		PRED(p, a, "-inum",     INUM,     v);
		PRED(p, a, "-links",    LINKS,    v);
		PRED(p, a, "-samefile", SAMEFILE, v);
		PRED(p, a, "-size",     SIZE,     v);

		fprintf(stderr, "unrecognized predicate `%s'\n", a);
		exit(1);
	}

	while (p->top > 0) {
		if (p->debug)
			printf("despooling [%i] (%02x)\n", p->top, p->ops[p->top - 1]);
		s_push_rpn(p, make_oper(p->ops[--p->top]));
	}

	return s_resolve(p);
}

static int s_eval_type(struct stat *st, char t)
{
	switch (t) {
	case 'b': return S_ISBLK  (st->st_mode);
	case 'c': return S_ISCHR  (st->st_mode);
	case 'd': return S_ISDIR  (st->st_mode);
	case 'p': return S_ISFIFO (st->st_mode);
	case 'f': return S_ISREG  (st->st_mode);
	case 'l': return S_ISLNK  (st->st_mode);
	case 's': return S_ISSOCK (st->st_mode);
	}
	return 0;
}

static int s_eval(expr_t *e, FTSENT *f)
{
	switch (e->op) {
	case OP_AND:  return s_eval(e->L, f) && s_eval(e->R, f);
	case OP_OR:   return s_eval(e->L, f) || s_eval(e->R, f);
	case OP_NOT:  return !s_eval(e->L, f);
	}

	switch (e->pr->type) {
	case PR_MAXDEPTH: return f->fts_level <= e->pr->arg.i64;
	case PR_MINDEPTH: return f->fts_level >= e->pr->arg.i64;

	case PR_AMIN:   return compare(e->pr, f->fts_statp->st_atime, e->pr->arg.i64);
	case PR_ATIME:  return compare(e->pr, f->fts_statp->st_atime, e->pr->arg.i64);
	case PR_ANEWER: return compare(e->pr, f->fts_statp->st_atime, e->pr->arg.stat.st_atime);

	case PR_MMIN:   return compare(e->pr, f->fts_statp->st_mtime, e->pr->arg.i64);
	case PR_MTIME:  return compare(e->pr, f->fts_statp->st_mtime, e->pr->arg.i64);
	case PR_MNEWER: return compare(e->pr, f->fts_statp->st_mtime, e->pr->arg.stat.st_mtime);

	case PR_CMIN:   return compare(e->pr, f->fts_statp->st_ctime, e->pr->arg.i64);
	case PR_CTIME:  return compare(e->pr, f->fts_statp->st_ctime, e->pr->arg.i64);
	case PR_CNEWER: return compare(e->pr, f->fts_statp->st_ctime, e->pr->arg.stat.st_ctime);

	case PR_EMPTY:  return f->fts_statp->st_size == 0;

	case PR_TRUE:   return 1;
	case PR_FALSE:  return 0;

	case PR_XTYPE:
	case PR_TYPE:  return s_eval_type(f->fts_statp, e->pr->arg.string[0]);

	case PR_USER:
	case PR_UID:   return f->fts_statp->st_uid == e->pr->arg.i64;

	case PR_GROUP:
	case PR_GID:   return f->fts_statp->st_gid == e->pr->arg.i64;

	case PR_LNAME:
	case PR_NAME:   return fnmatch(e->pr->arg.string, f->fts_name, 0) == 0;

	case PR_ILNAME:
	case PR_INAME:  return fnmatch(e->pr->arg.string, f->fts_name, FNM_CASEFOLD) == 0;

	case PR_PATH:   return fnmatch(e->pr->arg.string, f->fts_path, 0) == 0;
	case PR_IPATH:  return fnmatch(e->pr->arg.string, f->fts_path, FNM_CASEFOLD) == 0;

	case PR_INUM:  return f->fts_statp->st_ino == e->pr->arg.i64;
	case PR_LINKS: return compare(e->pr, f->fts_statp->st_nlink, e->pr->arg.i64);

	case PR_READABLE: return access(f->fts_path, R_OK) == 0;
	case PR_WRITABLE: return access(f->fts_path, W_OK) == 0;

	case PR_SAMEFILE: return f->fts_statp->st_ino == e->pr->arg.stat.st_ino
	                      && f->fts_statp->st_dev == e->pr->arg.stat.st_dev;

	case PR_SIZE: return compare(e->pr, f->fts_statp->st_size, e->pr->arg.i64);
	}

	return 0;
}

static void s_expr_dump(expr_t *e, const char *prefix)
{
	if (!e) return;
	if (e->pr) {
		fprintf(stderr, "%s %s [%02x]\n", prefix, PR_TYPE_NAMES[e->pr->type], e->pr->type);
	} else {
		fprintf(stderr, "%s %s [%02x]\n", prefix, OP_NAMES[e->op], e->op);
	}
	char *new = malloc(strlen(prefix) + 5);
	memset(new, ' ', strlen(prefix) + 4);
	memcpy(new, prefix, strlen(prefix));
	s_expr_dump(e->L, new);
	s_expr_dump(e->R, new);
	free(new);
}

static void s_track(context_t *c, FTSENT *e)
{
	c->count++;
	if (c->count == 1) {
		c->size.min = c->size.max = e->fts_statp->st_size;
	} else {
		c->size.min = MIN(c->size.min, e->fts_statp->st_size);
		c->size.max = MAX(c->size.max, e->fts_statp->st_size);
	}
	c->size.sum += e->fts_statp->st_size;
}

static void s_usage(const char *bin)
{
	fprintf(stderr, "USAGE: %s /path/to/dir [check-options] -- [find-options]\n", bin);
	exit(1);
}

int main(int argc, char **argv)
{
	if (argc < 2)
		s_usage(argv[0]);

	context_t ctx = { 0 };
	ctx.track     = TRACK_COUNT;
	ctx.aggregate = AGGREGATE_SUM;

	ctx.path = strdup(argv[1]);
	if (ctx.path[0] == '-')
		s_usage(argv[0]);

	int i;
	for (i = 2; i < argc - 1; i++) {
		if (strcmp(argv[i], "--") == 0) {
			i++;
			break;
		}
		if (strcmp(argv[i], "-debug") == 0) {
			ctx.debug = 1;
			continue;
		}
		if (strcmp(argv[i], "-dumptree") == 0) {
			ctx.dumptree = 1;
			continue;
		}
		if (strcmp(argv[i], "-track") == 0) {
			i++; if (!argv[i]) s_usage(argv[0]);

			     if (strcasecmp(argv[i], "count") == 0) ctx.track = TRACK_COUNT;
			else if (strcasecmp(argv[i], "size")  == 0) ctx.track = TRACK_SIZE;
			else s_usage(argv[0]);
			continue;
		}
		if (strcmp(argv[i], "-aggregate") == 0 || strcmp(argv[i], "-aggr") == 0) {
			i++; if (!argv[i]) s_usage(argv[0]);

			if (strcasecmp(argv[i], "sum") == 0)
				ctx.aggregate = AGGREGATE_SUM;
			else if (strcasecmp(argv[i], "min") == 0 || strcasecmp(argv[i], "minimum") == 0)
				ctx.aggregate = AGGREGATE_MIN;
			else if (strcasecmp(argv[i], "max") == 0 || strcasecmp(argv[i], "maximum") == 0)
				ctx.aggregate = AGGREGATE_MAX;
			else if (strcasecmp(argv[i], "avg") == 0 || strcasecmp(argv[i], "average") == 0)
				ctx.aggregate = AGGREGATE_AVG;
			else s_usage(argv[0]);
		}
	}

	if (ctx.track == TRACK_COUNT && ctx.aggregate != AGGREGATE_SUM) {
		fprintf(stderr, "WARNING: you specified -track count with a -aggr %s, which makes no sense.  "
		                "falling back to -aggr sum\n",
			  ctx.aggregate == AGGREGATE_MIN ? "min"
			: ctx.aggregate == AGGREGATE_MAX ? "max"
			: ctx.aggregate == AGGREGATE_AVG ? "avg" : "<unknown>");
	}

	if (!argv[i])
		s_usage(argv[0]);

	parser_t p = { .i = i, .argc = argc, .argv = argv, .debug = ctx.dumptree };
	expr_t *root = s_parse(&p);
	if (ctx.dumptree) {
		fprintf(stderr, "\neval parse tree:\n");
		s_expr_dump(root, "");
		fprintf(stderr, "\n");
	}

	char * const paths[2] = { ctx.path, NULL };
	FTS *f = fts_open(paths, FTS_PHYSICAL, NULL);
	if (!f) {
		perror("fts_open");
		exit(1);
	}

	FTSENT *e;
	while ((e = fts_read(f)) != NULL) {
		if (e->fts_info & FTS_DP) continue;
		if (s_eval(root, e)) {
			if (ctx.debug) {
				fprintf(stderr, "found file `%s' [%lub]\n", e->fts_path, e->fts_statp->st_size);
			}
			s_track(&ctx, e);
		}
	}

	if (ctx.debug) {
		fprintf(stderr, "filesystem traversal complete\n");
		fprintf(stderr, "final stats:\n");
		fprintf(stderr, "  count:     %lu\n", ctx.count);
		fprintf(stderr, "  min(size): %lu\n", ctx.size.min);
		fprintf(stderr, "  max(size): %lu\n", ctx.size.max);
		fprintf(stderr, "  sum(size): %lu\n", ctx.size.sum);
		fprintf(stderr, "  avg(size): %f\n",  1.0 * ctx.size.sum / ctx.count);
	}

	PREFIX = fqdn();
	ts = time_s();
	if (ctx.track == TRACK_COUNT) {
		printf("SAMPLE %i %s:files:NAME %lu\n", ts, PREFIX, ctx.count);

	} else if (ctx.track == TRACK_SIZE) {
		printf("SAMPLE %i %s:files:NAME ", ts, PREFIX);

		switch (ctx.aggregate) {
		case AGGREGATE_SUM: printf("%lu\n", ctx.size.sum); break;
		case AGGREGATE_MIN: printf("%lu\n", ctx.size.min); break;
		case AGGREGATE_MAX: printf("%lu\n", ctx.size.max); break;
		case AGGREGATE_AVG: printf("%e\n",  1.0 * ctx.size.sum / ctx.count); break;
		default: printf("0\n");
		}

	} else {
		fprintf(stderr, "invalid -track option\n");
		return 1;
	}

	return 0;
}
