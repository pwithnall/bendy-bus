SUBDIRS = po

# Setup
bin_PROGRAMS =
lib_LTLIBRARIES =
EXTRA_DIST =
CLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =
GITIGNOREFILES =
BUILT_SOURCES =

# libdfsm

lib_LTLIBRARIES += libdfsm/libdfsm.la

libdfsm_main_header = libdfsm/dfsm.h
libdfsm_public_headers = \
	libdfsm/dfsm-ast.h \
	libdfsm/dfsm-utils.h \
	$(NULL)
libdfsm_sources = \
	libdfsm/dfsm-bison.y \
	libdfsm/dfsm-flex.l \
	libdfsm/dfsm-ast.c \
	libdfsm/dfsm-parser.c \
	libdfsm/dfsm-parser.h \
	$(NULL)

libdfsmincludedir = $(pkgincludedir)/dfsm
libdfsminclude_HEADERS = \
	$(libdfsm_main_header) \
	$(libdfsm_public_headers) \
	$(NULL)

libdfsm_libdfsm_la_SOURCES = \
	$(libdfsm_sources) \
	$(NULL)

libdfsm_libdfsm_la_YFLAGS = \
	--warnings=all \
	-d \
	--report=all \
	$(NULL)

libdfsm_libdfsm_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libdfsm \
	-DG_LOG_DOMAIN=\"libdfsm\" \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS) \
	$(NULL)

libdfsm_libdfsm_la_CFLAGS = \
	$(WARN_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libdfsm_libdfsm_la_LIBADD = \
	$(GLIB_LIBS) \
	$(AM_LIBADD) \
	$(NULL)

libdfsm_libdfsm_la_LDFLAGS = \
	-version-info $(LIBDFSM_LT_VERSION) \
	-export-symbols $(srcdir)/libdfsm/dfsm.symbols \
	-no-undefined \
	$(AM_LDFLAGS) \
	$(NULL)

EXTRA_DIST += libdfsm/dfsm.symbols

CLEANFILES += \
	libdfsm/dfsm-flex.c \
	libdfsm/libdfsm_libdfsm_la-dfsm-bison.c \
	libdfsm/libdfsm_libdfsm_la-dfsm-bison.h \
	libdfsm/libdfsm_libdfsm_la-dfsm-bison.output \
	$(NULL)

# Gubbins
ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST += \
	autogen.sh \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	README \
	COPYING \
	AUTHORS \
	INSTALL \
	NEWS \
	$(NULL)

CLEANFILES += \
	`find "$(srcdir)" -type f -name .dirstamp -print` \
	$(NULL)

DISTCLEANFILES += \
	intltool-extract \
	intltool-merge \
	intltool-update \
	$(NULL)

MAINTAINERCLEANFILES += \
	INSTALL \
	aclocal.m4 \
	compile \
	config.guess \
	config.h.in \
	config.sub \
	depcomp \
	install-sh \
	ltmain.sh \
	missing \
	mkinstalldirs \
	`find "$(srcdir)" -type f -name Makefile.in -print` \
	po/Makevars.template \
	po/Rules-quot \
	po/boldquot.sed \
	po/en@boldquot.header \
	po/en@quot.header \
	po/insert-header.sin \
	po/quot.sed \
	po/remove-potcdate.sin \
	$(NULL)

# ChangeLog generation
ChangeLog: $(srcdir)/ChangeLog
$(srcdir)/ChangeLog:
	@echo Creating $@
	@if test -d "$(srcdir)/.git"; then \
	  (GIT_DIR=$(top_srcdir)/.git ./missing --run \
	   git log --stat) | fmt --split-only > $@.tmp \
	  && mv -f $@.tmp $@ \
	  || ($(RM) $@.tmp; \
	      echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	      (test -f $@ || echo git-log is required to generate this file >> $@)); \
	else \
	  test -f $@ || \
	  (echo A git checkout and git-log is required to generate ChangeLog >&2 && \
	  echo A git checkout and git-log is required to generate this file >> $@); \
	fi
.PHONY: $(srcdir)/ChangeLog

MAINTAINERCLEANFILES += ChangeLog

-include $(top_srcdir)/git.mk
