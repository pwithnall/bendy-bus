# autofoo
AC_INIT([Bendy Bus],m4_esyscmd([./git-version-gen .tarball-version]),[],[bendy-bus],[])
AM_INIT_AUTOMAKE([check-news dist-bzip2 no-dist-gzip])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE([enable])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# Toolchain
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CPP
AM_PROG_LEX
AC_PROG_YACC
LT_INIT

AC_PATH_PROG([GLIB_GENMARSHAL],[glib-genmarshal])

# Internationalisation
GETTEXT_PACKAGE=bendy-bus
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,["$GETTEXT_PACKAGE"],[Define to the Gettext package name])
IT_PROG_INTLTOOL([0.40.0])

# Before making a release, the DFSM_LT_VERSION string should be modified. The string is of the form c:r:a. Follow these instructions sequentially:
#
#  1. If the library source code has changed at all since the last update, then increment revision (‘c:r:a’ becomes ‘c:r+1:a’).
#  2. If any interfaces have been added, removed, or changed since the last update, increment current, and set revision to 0.
#  3. If any interfaces have been added since the last public release, then increment age.
#  4. If any interfaces have been removed or changed since the last public release, then set age to 0.
DFSM_LT_VERSION=0:0:0
AC_SUBST(DFSM_LT_VERSION)

# Dependencies
LT_LIB_M

GLIB_REQS=2.30.0
GIO_REQS=2.0.0

PKG_CHECK_MODULES([GLIB],[glib-2.0 >= $GLIB_REQS])
PKG_CHECK_MODULES([GIO],[gio-2.0 >= $GIO_REQS gio-unix-2.0 >= $GIO_REQS])

# Output!
AC_CONFIG_FILES([
Makefile
po/Makefile.in
])
AC_OUTPUT
