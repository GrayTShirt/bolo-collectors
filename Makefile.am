#
# Makefile.am - Bolo Collectors Automake File
#

AUTOMAKE_OPTIONS = foreign subdir-objects

EXTRA_DIST  =
EXTRA_DIST += bootstrap

collectorsdir=$(libdir)/bolo/collectors
dist_collectors_SCRIPTS  =
dist_collectors_SCRIPTS += cogd
dist_collectors_SCRIPTS += hostinfo
dist_collectors_SCRIPTS += process
dist_collectors_SCRIPTS += sar

############################################################

version:
	@echo $(VERSION)

distfile: dist
	@echo $(PACKAGE)-$(VERSION).tar.gz

manifest:
	@echo >&2 "nothin doin"

test: check
