#
# Makefile.am - Bolo Collectors Automake File
#

AUTOMAKE_OPTIONS = foreign subdir-objects

EXTRA_DIST  =
EXTRA_DIST += bootstrap

LDADD = -lvigor -lcurl

collectorsdir=$(libdir)/bolo/collectors
dist_collectors_SCRIPTS  =
dist_collectors_SCRIPTS += cogd
dist_collectors_SCRIPTS += hostinfo
dist_collectors_SCRIPTS += process
dist_collectors_SCRIPTS += sar

collectors_PROGRAMS = linux httpd
linux_SOURCES = src/linux.c
httpd_SOURCES = src/httpd.c

############################################################

version:
	@echo $(VERSION)

distfile: dist
	@echo $(PACKAGE)-$(VERSION).tar.gz

manifest:
	@echo >&2 "nothin doin"

test: check
