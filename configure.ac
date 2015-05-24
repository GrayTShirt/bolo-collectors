#
#  Copyright 2015 James Hunt <james@jameshunt.us>
#
#  This file is part of Bolo.
#
#  Bolo is free software: you can redistribute it and/or modify it under the
#  terms of the GNU General Public License as published by the Free Software
#  Foundation, either version 3 of the License, or (at your option) any later
#  version.
#
#  Bolo is distributed in the hope that it will be useful, but ENABLEOUT ANY
#  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
#  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
#  details.
#
#  You should have received a copy of the GNU General Public License along
#  with Bolo.  If not, see <http://www.gnu.org/licenses/>.
#

AC_PREREQ(2.63)
AC_INIT([bolo-collectors], [0.2.1], [bugs@niftylogic.com])

AC_CONFIG_SRCDIR([src/linux.c])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build])

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

AC_PROG_CC

AC_HAVE_LIBRARY(vigor,,    AC_MSG_ERROR(Missing libvigor - see http://github.com/filefrog/libvigor))

build_ALL=auto
AC_ARG_WITH([all-collectors],
	[AS_HELP_STRING([--with-all-collectors],
		[Build all optional collector programs (--with-X-collector)])],
	[echo "triggered build_ALL logic"
	 case "${withval}" in
	 yes)  build_ALL=yes;  AC_MSG_NOTICE([Will build ALL optional collectors])            ;;
	 no)   build_ALL=no;   AC_MSG_NOTICE([Will not build any optional collectors])        ;;
	 auto) build_ALL=auto; AC_MSG_NOTICE([Will attempt to build ALL optional collectors]) ;;
	 *)    AC_MSG_ERROR([bad value ${withval} for --with-all-collectors]) ;;
	 esac],
	[build_ALL=auto])

BOLO_COLLECTOR([rrdcache], [auto], [metrics from an RRDtool write-caching daemon],
	[#define HAVE_STDINT_H
	 #include <rrd.h>
	 #include <rrd_client.h>
	 int main() { return 0; }
	], [-lrrd])

BOLO_COLLECTOR([fw], [auto], [metrics from a host-based IP firewall],
	[#include <libiptc/libiptc.h>
	 int main() { return 0; }
	], [-lip4tc -lip6tc])

BOLO_COLLECTOR([postgres], [auto], [metrics by running PostgreSQL database queries],
	[#include <postgresql/libpq-fe.h>
	 int main() { return 0; }
	], [-lpq])

BOLO_COLLECTOR([mysql], [auto], [metrics by running MySQL database queries],
	[#include <mysql.h>
	 int main() { return 0; }
	], [-lmysqlclient $(which_mysql_config >/dev/null 2>&1 && mysql_config --cflags)])

BOLO_COLLECTOR([httpd], [auto], [metrics from web server daemons],
	[#include <curl/curl.h>
	 int main() { return 0; }
	], [-lcurl])


AC_CONFIG_FILES([Makefile])
AC_OUTPUT
