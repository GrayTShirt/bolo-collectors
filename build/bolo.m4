dnl =============================================================
dnl BOLO_COLLECTOR(name, default, description, test-prog, libs)
dnl
dnl Sets up --with-X-collector and --without-X-collector
dnl arguments to ./configure, which can be used to include
dnl or exclude specific collectors.
dnl
dnl Parameters:
dnl   name:        Name of the collector; used for generating the
dnl                command-line flag and the Makefile.am variable
dnl
dnl   default:     Default value for the argument, one of 'yes',
dnl                'no' or 'auto'
dnl
dnl   description: A string to be used in the ./configure --help
dnl                screen, which completes the sentence
dnl                "for collecting ..."
dnl
dnl   test-prog:   A test C program that will be used to determine
dnl                if the host platform has the correct header files
dnl                and dynamic libraries to compile the collector.
dnl
dnl                This is used in 'auto' mode to determine if the
dnl                collector should be built, and in 'yes' mode to
dnl                ensure that the user's request can be honored.
dnl
dnl   libs:        Additional library flags for compiling the test
dnl                program, i.e. [-lm -lrt -lvigor]
dnl
dnl
dnl Each call to BOLO_COLLECTOR() will set up an Automake variable
dnl named 'build_X_collector', which can be used to conditionally
dnl enable compile targets and update the collectors_PROGRAMS var.
dnl
dnl =============================================================
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
			[AC_LINK_IFELSE([AC_LANG_SOURCE([$4])],
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

	 AM_CONDITIONAL([build_$1_collector], [test "x$build_$1_collector" != "xno"])
	])

dnl =============================================================
dnl BOLO_WITH(name, description)
dnl
dnl Sets up --with-X and --without-X arguments to ./configure, and
dnl the necessary handler code to update both CFLAGS and LDFLAGS,
dnl based on arguments and on-disk directories.
dnl
dnl The basic principle is this: if the caller gives us a path
dnl to one of these --with-X arguments, we look for library (lib/)
dnl and header file (include/) directories under the path, and adjust
dnl LDFLAGS and CFLAGS accordingly with new -L and -I flags.
dnl
dnl =============================================================
AC_DEFUN([BOLO_WITH],
	# Set up CFLAGS and LDFLAGS using the --with-X=<path> idiom
	[AC_ARG_WITH([$1],
		[AS_HELP_STRING([--with-$1],[Where to find $2])],
		[if test $withval != "yes" -a $withval != "no"; then
			prefix=${withval%%/}
			if test -d "$prefix/lib/";     then LDFLAGS="$LDFLAGS -L$prefix/lib";   fi
			if test -d "$prefix/include/"; then CFLAGS="$CFLAGS -I$prefix/include"; fi
		fi],[])
	])
