#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="bendy-bus"

(test -f $srcdir/configure.ac) || {
	echo -n "**Error**: Directory ‘$srcdir’ does not look like the top-level $PKG_NAME directory"
	exit 1
}

libtoolize --force --copy || exit 1
intltoolize --force --copy --automake || exit 1
aclocal -I m4 || exit 1
autoconf || exit 1
autoheader || exit 1
automake --gnu --add-missing --force --copy -Wno-portability || exit 1

$srcdir/configure $conf_flags "$@" && echo "Now type ‘make’ to compile $PKG_NAME" || exit 1
