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

AC_DEFUN([BOLO_COLLECTOR],
	# Set up the --with-X-collector argument handler
	[AC_ARG_WITH([$1-collector],
		[AS_HELP_STRING([--with-$1-collector],
			[Build the `$1` collector, for collecting $3 (default is $2)])],
		[case "${withval}" in
		 yes)  build_$1_collector=yes;  AC_MSG_NOTICE([Will build the $1 collector])            ;;
		 no)   build_$1_collector=no;   AC_MSG_NOTICE([Will not build the $1 collector])        ;;
		 auto) build_$1_collector=auto; AC_MSG_NOTICE([Will attempt to build the $1 collector]) ;;
		 *)    AC_MSG_ERROR([bad value ${withval} for --with-$1-collector]) ;;
		 esac],
		[build_$1_collector=$build_ALL])

	# Under 'yes' and 'auto', try to determine if we meet our
	# pre-requisites (from both header and library perspectives)
	 AS_IF([test "x$build_$1_collector" != "xno"],
		[bolo_prereqs_save_LIBS=$LIBS
		 LIBS="$5 $LIBS"
		 AC_MSG_CHECKING([for `$1` collector prereqs])
		 AC_CACHE_VAL([bolo_cv_$1_ok],
			[AC_LINK_IFELSE([$4],
			[bolo_cv_$1_ok=yes],
			[bolo_cv_$1_ok=no])])
		 AC_MSG_RESULT([$bolo_cv_$1_ok])
		 LIBS=$bolo_prereqs_save_LIBS

		 # If we don't have the support we need...
		 AS_IF([test "x$bolo_cv_$1_ok" = "xno"],
			AS_IF([test "x$build_$1_collector" = "xyes"],
				# And we specified --with-X=yes, it's a hard failure
				[AC_MSG_ERROR([--with-$1-collector: no support for $1 collector])],
				# ... otherwise autodetection set to no
				[build_$1_collector=no]))
		])

	 AM_CONDITIONAL([build_$1_collector], [test "x$build_$1_collector" = "xyes"])
	])


AC_PREREQ(2.63)
AC_INIT([bolo-collectors], [0.1.0], [bugs@niftylogic.com])

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
	[#include <rrd.h>
	 #include <rrd_client.h>
	 int main() { return 0 }
	], [-lrrd])

BOLO_COLLECTOR([fw], [auto], [metrics from a host-based IP firewall],
	[#include <libiptc/libiptc.h>
	 int main() { return 0 }
	], [-lip4tc -lip5tc])

BOLO_COLLECTOR([postgres], [auto], [metrics by running PostgreSQL database queries],
	[#include <postgresql/libpq-fe.h>
	 int main() { return 0 }
	], [-lpq])

BOLO_COLLECTOR([httpd], [auto], [metrics from web server daemons],
	[#include <curl/curl.h>
	 int main() { return 0 }
	], [-lcurl])


AC_CONFIG_FILES([Makefile])
AC_OUTPUT
