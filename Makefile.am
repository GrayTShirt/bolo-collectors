#
# Makefile.am - Bolo Collectors Automake File
#

AUTOMAKE_OPTIONS = foreign subdir-objects

EXTRA_DIST  =
EXTRA_DIST += bootstrap

LDADD = -lvigor

collectorsdir=$(libdir)/bolo/collectors
dist_collectors_SCRIPTS  =
dist_collectors_SCRIPTS += cogd
dist_collectors_SCRIPTS += df
dist_collectors_SCRIPTS += hostinfo
dist_collectors_SCRIPTS += openfiles
dist_collectors_SCRIPTS += process
dist_collectors_SCRIPTS += sar

collectors_PROGRAMS = procfs df linux
procfs_SOURCES = src/procfs.c
df_SOURCES = src/df.c
linux_SOURCES = src/linux.c

############################################################

version:
	@echo $(VERSION)

distfile: dist
	@echo $(PACKAGE)-$(VERSION).tar.gz

manifest:
	@echo >&2 "nothin doin"

test: check
