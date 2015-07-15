#
# Makefile.am - Bolo Collectors Automake File
#

AUTOMAKE_OPTIONS = foreign subdir-objects
ACLOCAL_AMFLAGS  = -I build

EXTRA_DIST  =
EXTRA_DIST += bootstrap

LDADD = -lvigor

collectorsdir=$(libdir)/bolo/collectors
dist_collectors_SCRIPTS  =
dist_collectors_SCRIPTS += cogd
dist_collectors_SCRIPTS += hostinfo
dist_collectors_SCRIPTS += nagwrap
dist_collectors_SCRIPTS += process

collectors_PROGRAMS = linux files tcp

files_SOURCES    = src/files.c src/common.h
linux_SOURCES    = src/linux.c src/common.h
tcp_SOURCES      = src/tcp.c   src/common.h

if build_httpd_collector
collectors_PROGRAMS += httpd
httpd_SOURCES    = src/httpd.c src/common.h
httpd_LDADD      = -lcurl
endif

if build_fw_collector
collectors_PROGRAMS += fw
fw_SOURCES       = src/fw.c src/common.h
fw_LDADD         = -lip4tc -lip6tc
endif

if build_rrdcache_collector
collectors_PROGRAMS += rrdcache
rrdcache_SOURCES = src/rrdcache.c src/common.h
rrdcache_LDADD   = -lrrd
endif

if build_postgres_collector
collectors_PROGRAMS += postgres
postgres_SOURCES = src/postgres.c src/common.h
postgres_LDADD   = -lpq
endif

if build_mysql_collector
collectors_PROGRAMS += mysql
mysql_SOURCES    = src/mysql.c src/common.h
mysql_LDADD      = -ldl
endif

############################################################

version:
	@echo $(VERSION)

distfile: dist
	@echo $(PACKAGE)-$(VERSION).tar.gz

manifest:
	@echo >&2 "nothin doin"

test: check
