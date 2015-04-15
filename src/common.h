/* common.h */
#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <vigor.h>

#include <sys/types.h>
#include <sys/stat.h>

#define streq(a,b) (strcmp((a), (b)) == 0)

#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define MAX(a,b) ((a) < (b) ? (b) : (a))

static char *PREFIX = 0;
static int32_t ts = 0;
