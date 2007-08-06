AC_INIT(abacuscm, 1.0, jaco@kroon.co.za)
AC_CONFIG_SRCDIR([src/mainwindow.C])
AM_INIT_AUTOMAKE([1.7 dist-bzip2 subdir-objects foreign])
AM_CONFIG_HEADER(config.h)

dnl Checks for programs
AC_PROG_CC_STDC
AC_PROG_CXX
AM_PROG_CC_C_O
AC_PROG_CPP

AC_LIBLTDL_INSTALLABLE
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_MKDIR_P
PKG_PROG_PKG_CONFIG
AC_CONFIG_SUBDIRS(libltdl)
AC_SUBST(LIBTOOL_DEPS)
AC_SUBST(LTDLINCL)
AC_SUBST(LIBLTDL)

dnl Checks for libraries
AC_CHECK_LIB([pthread], [pthread_create],, [AC_MSG_ERROR([cannot find pthread])])
AC_CHECK_LIB([ssl], [SSL_connect],, [AC_MSG_ERROR([cannot find OpenSSL])])
AC_CHECK_LIB([mysqlclient_r], [mysql_real_connect], [have_mysql=yes], [have_mysql=no])
PKG_CHECK_MODULES([QT], [qt-mt >= 3.0], [have_qt=yes], [have_qt=no])

dnl Checks for header files
AC_HEADER_STDC
AC_CHECK_HEADERS([mysql/mysql.h])
AC_LANG_PUSH(C++)
AC_CHECK_HEADERS([boost/shared_ptr.hpp])
AC_LANG_POP(C++)

dnl Checks for compiler characteristics
AC_C_INLINE
AC_C_VOLATILE

dnl Checks for library functions
AC_CHECK_FUNCS([strndup])

dnl Checks for system services
dnl Bleah. Qt3 doesn't seem to have a nice way to get the 'right' path to
dnl moc and uic to avoid picking up the Qt4 versions. We use the first match
dnl from this list
dnl 1. exec_prefix from pkg-config
dnl 2. $QTDIR/bin
dnl 3. $PATH
dnl
dnl However, if --with-qt=<path> is passed, it is used regardless

qtdir_user=
AC_ARG_WITH([qt], AC_HELP_STRING([--with-qt=PATH], [parent of directory containing uic and moc for Qt3]), [test "$withval" != no && qtdir_user="$withval"])
if test "x$qtdir_user" = "x"; then
	qtdir_pkg=`$PKG_CONFIG --atleast-version=3.0 --variable=exec_prefix`
	MOC=
	UIC=
	if test "x$qtdir_pkg" != "x"; then
		AC_PATH_PROG([MOC], [moc],, [$qtdir_pkg/bin])
		AC_PATH_PROG([UIC], [uic],, [$qtdir_pkg/bin])
	fi
	if test "x$QTDIR" != "x"; then
		AC_PATH_PROG([MOC], [moc],, [$QTDIR/bin])
		AC_PATH_PROG([UIC], [uic],, [$QTDIR/bin])
	fi
	AC_PATH_PROG([MOC], [moc], [no])
	AC_PATH_PROG([UIC], [uic], [no])
else
	MOC="$qtdir_user/bin/moc"
	UIC="$qtdir_user/bin/uic"
fi

AC_PATH_PROG([PDFLATEX], [pdflatex], [no])

do_client=yes
do_server=yes
do_marker=yes
do_manual=yes
AC_ARG_ENABLE([client], AC_HELP_STRING([--disable-client], [do not build the client]), [test $enableval = no && do_client="no (disabled by user)"])
AC_ARG_ENABLE([server], AC_HELP_STRING([--disable-server], [do not build the server]), [test $enableval = no && do_server="no (disabled by user)"])
AC_ARG_ENABLE([marker], AC_HELP_STRING([--disable-marker], [do not build the marker]), [test $enableval = no && do_marker="no (disabled by user)"])
AC_ARG_ENABLE([manual], AC_HELP_STRING([--disable-manual], [do not build the user manual]), [test $enableval = no && do_manual="no (disabled by user)"])

if test "$do_client" = yes && test "$have_qt" = no; then
	do_client="no (QT3 not found)"
fi
if test "$do_client" = yes && test "$UIC" = no; then
	do_client="no (QT found but uic missing)"
fi
if test "$do_client" = yes && test "$MOC" = no; then
	do_client="no (QT found but moc missing)"
fi
if test "$do_server" = yes && test "$ac_cv_header_boost_shared_ptr_hpp" = no; then
	do_server="no (<boost/shared_ptr.hpp> not found)"
fi
if test "$do_server" = yes && test "$ac_cv_header_mysql_h" = no; then
	do_server="no (<mysql.h> not found)"
fi
if test "$do_server" = yes && test "$have_mysql" = no; then
	do_server="no (-lmysqlclient_r not found)"
fi
if test "$do_manual" = yes && test "$PDFLATEX" = no; then
	do_manual="no (pdflatex not found)"
fi

AM_CONDITIONAL(DO_CLIENT, test "$do_client" = yes)
AM_CONDITIONAL(DO_SERVER, test "$do_server" = yes)
AM_CONDITIONAL(DO_MARKER, test "$do_marker" = yes)
AM_CONDITIONAL(DO_MANUAL, test "$do_manual" = yes)

AC_CONFIG_FILES([Makefile doc/Makefile doc/usermanual/Makefile])
AC_OUTPUT

dnl Show configuration
echo
echo "Configuration:"
echo "        client: $do_client"
echo "        server: $do_server"
echo "        marker: $do_marker"
echo "        manual: $do_manual"
