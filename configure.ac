# Release version
m4_define(version_major, 0)
m4_define(version_minor, 1)
m4_define(version_micro, 0)

# autofoo
AC_INIT([D-Bus simulator],[version_major.version_minor.version_micro],[],[dbus-simulator],[])
AM_INIT_AUTOMAKE([check-news dist-bzip2 no-dist-gzip subdir-objects])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE([enable])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# Toolchain
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CPP
LT_INIT

# Internationalisation
GETTEXT_PACKAGE=dbus-simulator
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,["$GETTEXT_PACKAGE"],[Define to the Gettext package name])
IT_PROG_INTLTOOL([0.40.0])

# Before making a release, the DFSM_LT_VERSION string should be modified. The string is of the form c:r:a. Follow these instructions sequentially:
#
#  1. If the library source code has changed at all since the last update, then increment revision (‘c:r:a’ becomes ‘c:r+1:a’).
#  2. If any interfaces have been added, removed, or changed since the last update, increment current, and set revision to 0.
#  3. If any interfaces have been added since the last public release, then increment age.
#  4. If any interfaces have been removed or changed since the last public release, then set age to 0.
LIBDFSM_LT_VERSION=0:0:0
AC_SUBST(LIBDFSM_LT_VERSION)

# Dependencies
GLIB_REQS=2.0.0

PKG_CHECK_MODULES([GLIB],[glib-2.0 >= $GLIB_REQS])

# Output!
AC_CONFIG_FILES([
Makefile
po/Makefile.in
])
AC_OUTPUT
