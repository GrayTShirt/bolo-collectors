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
dist_collectors_SCRIPTS += hostinfo
dist_collectors_SCRIPTS += process

collectors_PROGRAMS = linux httpd fw
linux_SOURCES = src/linux.c
httpd_SOURCES = src/httpd.c
httpd_LDADD   = -lcurl
fw_SOURCES    = src/fw.c
fw_LDADD      = -lip4tc -lip6tc

############################################################

version:
	@echo $(VERSION)

distfile: dist
	@echo $(PACKAGE)-$(VERSION).tar.gz

manifest:
	@echo >&2 "nothin doin"

test: check
