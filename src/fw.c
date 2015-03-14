#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vigor.h>
#include <libiptc/libiptc.h>

static const char *PREFIX;
static int32_t ts;

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
	if (strcmp(m->u.user.name, "comment") == 0
	 && strcmp((char *) m->data, r->comment) == 0) {

		printf("RATE %i %s:fw:%s:%s:%s.bytes   %llu\n", ts, PREFIX, r->table, r->chain, r->comment, e->counters.bcnt);
		printf("RATE %i %s:fw:%s:%s:%s.packets %llu\n", ts, PREFIX, r->table, r->chain, r->comment, e->counters.pcnt);
	}
	return 0;
}

int main(int argc, char **argv)
{
	PREFIX = fqdn();

	if (argc == 0) {
		fprintf(stderr, "USAGE: %s filter:CHAIN:comment ...\n", argv[0]);
		exit(1);
	}

	hash_t tables = { 0 };
	LIST(rules);

	rule_t *rule;
	int i, error = 0;
	for (i = 1; i < argc; i++) {
		rule = s_parse_rule(argv[i]);
		if (!rule) {
			fprintf(stderr, "invalid rule '%s'\n", argv[i]);
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

	char *name;
	struct xtc_handle *table;
	for_each_key_value(&tables, name, table) {
		rule_t *rule;
		const char *chain;
		const struct ipt_entry *entry;

		for (chain = iptc_first_chain(table); chain; chain = iptc_next_chain(table)) {
			for_each_object(rule, &rules, l) {
				if (strcmp(rule->table, name)  != 0
				 || strcmp(rule->chain, chain) != 0)
					continue;

				for (entry = iptc_first_rule(chain, table); entry; entry = iptc_next_rule(entry, table))
					IPT_MATCH_ITERATE(entry, s_matcher, entry, rule);
			}
		}
	}
	return 0;
}
