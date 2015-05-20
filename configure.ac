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
#  Bolo is distributed in the hope that it will be useful, but WITHOUT ANY
#  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
#  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
#  details.
#
#  You should have received a copy of the GNU General Public License along
#  with Bolo.  If not, see <http://www.gnu.org/licenses/>.
#

AC_PREREQ(2.63)
AC_INIT([bolo-collectors], [0.1.0], [bugs@niftylogic.com])

AC_CONFIG_SRCDIR([src/linux.c])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build])

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

AC_PROG_CC

AC_HAVE_LIBRARY(vigor,,    AC_MSG_ERROR(Missing libvigor - see http://github.com/filefrog/libvigor))
AC_HAVE_LIBRARY(curl,,     AC_MSG_ERROR(Missing cURL library))
AC_HAVE_LIBRARY(rrd,,      AC_MSG_ERROR(Missing RRD library))
AC_HAVE_LIBRARY(pq,,       AC_MSG_ERROR(Missing Postgres library))
AC_HAVE_LIBRARY(ip4tc,,    AC_MSG_ERROR(Missing iptables v4 library))
AC_HAVE_LIBRARY(ip6tc,,    AC_MSG_ERROR(Missing iptables v6 library))

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
