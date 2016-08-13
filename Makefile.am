#
# Makefile.am - Bolo Collectors Automake File
#

AUTOMAKE_OPTIONS = foreign subdir-objects
ACLOCAL_AMFLAGS  = -I build

EXTRA_DIST  =
EXTRA_DIST += bootstrap share version.sh
EXTRA_DIST += lib/Bolo/Collector.pm

LDADD = $(VIGOR_LIBS)

bin_PROGRAMS = rrdq
rrdq_SOURCES = src/rrdq.c
rrdq_LDADD   = -lrrd $(RRDQ_LIBS)

sharedir = $(prefix)/share/@PACKAGE@
share_DATA = share/pg.sql

collectorsdir=$(libdir)/bolo/collectors
dist_collectors_SCRIPTS  =
dist_collectors_SCRIPTS += cogd
dist_collectors_SCRIPTS += hostinfo
dist_collectors_SCRIPTS += nagwrap
dist_collectors_SCRIPTS += process
dist_collectors_SCRIPTS += snmp/snmp_cisco
dist_collectors_SCRIPTS += snmp/snmp_ifaces
dist_collectors_SCRIPTS += snmp/snmp_system
dist_collectors_SCRIPTS += snmp/snmp_cisco_detect
dist_collectors_SCRIPTS += snmp/snmp_cisco_ifaces
dist_collectors_SCRIPTS += snmp/snmp_cisco_sys

collectors_PROGRAMS = linux files tcp netstat

files_SOURCES    = src/files.c src/common.h
linux_SOURCES    = src/linux.c src/common.h
linux_LDADD      = $(LINUX_LIBS) $(VIGOR_LIBS)
tcp_SOURCES      = src/tcp.c   src/common.h
tcp_LDADD        = -lpthread $(VIGOR_LIBS)
netstat_SOURCES  = src/netstat.c   src/common.h

if build_httpd_collector
collectors_PROGRAMS += httpd
httpd_SOURCES    = src/httpd.c src/common.h
httpd_LDADD      = -lcurl $(VIGOR_LIBS)
endif

if build_fw_collector
collectors_PROGRAMS += fw
fw_SOURCES       = src/fw.c src/common.h
fw_LDADD         = -lip4tc -lip6tc $(VIGOR_LIBS)
endif

if build_rrdcache_collector
collectors_PROGRAMS += rrdcache
rrdcache_SOURCES = src/rrdcache.c src/common.h
rrdcache_LDADD   = -lrrd $(VIGOR_LIBS)
endif

if build_postgres_collector
collectors_PROGRAMS += postgres
postgres_SOURCES = src/postgres.c src/common.h
postgres_LDADD   = -lpq $(VIGOR_LIBS)
endif

if build_mysql_collector
collectors_PROGRAMS += mysql
mysql_SOURCES    = src/mysql.c src/common.h
mysql_LDADD      = -ldl $(VIGOR_LIBS)
endif

############################################################

install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(PERLDIR)/Bolo
	$(INSTALL_DATA) $(srcdir)/lib/Bolo/Collector.pm $(DESTDIR)$(PERLDIR)/Bolo/Collector.pm

uninstall-local:
	rm -f $(DESTDIR)$(PERLDIR)/Bolo/Collector.pm
	rmdir $(DESTDIR)$(PERLDIR)/Bolo

############################################################

version:
	@echo $(VERSION)

distfile: dist
	@echo $(PACKAGE)-$(VERSION).tar.gz

manifest:
	@echo >&2 "nothin doin"

test: check
