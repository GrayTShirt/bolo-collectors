#include "common.h"
#include <libiptc/libiptc.h>

typedef struct {
	list_t l;

	const char *table;
	const char *chain;
	const char *comment;

	char *_data;
} rule_t;

static rule_t* s_parse_rule(const char *s)
{
	rule_t *rule = vmalloc(sizeof(rule_t));
	rule->_data = strdup(s);
	list_init(&rule->l);

	rule->chain = rule->comment = NULL;
	char *p = rule->_data;
	rule->table = rule->_data;

	while (*p) {
		if (*p == ':') {
			*p++ = '\0';
			     if (!rule->chain)   rule->chain   = p;
			else if (!rule->comment) rule->comment = p;

		} else  {
			p++;
		}
	}

	if (!rule->chain || !rule->comment) {
		free(rule->_data);
		free(rule);
		return NULL;
	}

	return rule;
}

static int s_matcher(const struct ipt_entry_match *m, const struct ipt_entry *e, rule_t *r)
{
	if (streq(m->u.user.name, "comment")
	 && streq((char *) m->data, r->comment)) {

		printf("RATE %i %s:fw:%s:%s:%s.bytes   %llu\n", ts, PREFIX, r->table, r->chain, r->comment, e->counters.bcnt);
		printf("RATE %i %s:fw:%s:%s:%s.packets %llu\n", ts, PREFIX, r->table, r->chain, r->comment, e->counters.pcnt);
	}
	return 0;
}

char **RULES;
int parse_options(int argc, char **argv);

int main(int argc, char **argv)
{
	if (parse_options(argc, argv) != 0) {
		fprintf(stderr, "USAGE: %s [options] rule [rule ...]\n", argv[0]);
		exit(1);
	}
	assert(RULES[0]);

	hash_t tables = { 0 };
	LIST(rules);

	rule_t *rule;
	int i, error = 0;
	for (i = 0; RULES[i]; i++) {
		rule = s_parse_rule(RULES[i]);
		if (!rule) {
			fprintf(stderr, "invalid rule '%s'\n", RULES[i]);
			error++;
			continue;
		}

		if (!hash_get(&tables, rule->table)) {
			struct xtc_handle *handle = iptc_init(rule->table);
			assert(handle);
			hash_set(&tables, rule->table, handle);
		}

		list_push(&rules, &rule->l);
	}
	if (error)
		return 1;

	ts = time_s();
	char *name;
	struct xtc_handle *table;
	for_each_key_value(&tables, name, table) {
		rule_t *rule;
		const char *chain;
		const struct ipt_entry *entry;

		for (chain = iptc_first_chain(table); chain; chain = iptc_next_chain(table)) {
			for_each_object(rule, &rules, l) {
				if (!streq(rule->table, name)
				 || !streq(rule->chain, chain))
					continue;

				for (entry = iptc_first_rule(chain, table); entry; entry = iptc_next_rule(entry, table))
					IPT_MATCH_ITERATE(entry, s_matcher, entry, rule);
			}
		}
	}
	return 0;
}

int parse_options(int argc, char **argv)
{
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

		if (streq(argv[i], "-h") || streq(argv[i], "-?") || streq(argv[i], "--help")) {
			fprintf(stdout, "fw (a Bolo collector)\n"
			                "USAGE: fw [options] rule [rule ...]\n"
			                "\n"
			                "options:\n"
			                "   -h, --help               Show this help screen\n"
			                "   -p, --prefix PREFIX      Use the given metric prefix\n"
			                "                            (FQDN is used by default)\n"
			                "\n"
			                "Rules must be specified in the form\n"
			                "\n"
			                "    filter:CHAIN:comment\n"
			                "\n"
			                "At least one rule must be specified; all will be evaluated\n"
			                "together, in parallel\n"
			                "You may need to quote these, if <comment> contains spaces.\n"
			                "\n"
			                "To add a comment to an iptable rule you will need add the\n"
			                "following command line switches to your iptables call\n"
			                "    -m comment --comment \"limit ssh access\"\n"
			                "A full example:\n"
			                "    iptables -A INPUT -j DROP -p tcp --dport 22 -m comment --comment \"limit ssh access\"\n"
			                "\n"
			                "EXAMPLE:\n"
			                "    fw filter:INPUT:\"limit ssh access\"\n"
			                "\n");
			exit(0);
		}

		break;
	}

	INIT_PREFIX();

	RULES = argv + i;
	return argv[i] == NULL ? 1 : 0;
}
