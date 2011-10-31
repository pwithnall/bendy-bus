SUBDIRS = po

# Setup
bin_PROGRAMS =
noinst_PROGRAMS =
lib_LTLIBRARIES =
EXTRA_DIST =
CLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =
GITIGNOREFILES =
BUILT_SOURCES =

# libdfsm

lib_LTLIBRARIES += dfsm/libdfsm.la

dfsm_main_header = dfsm/dfsm.h
dfsm_public_headers = \
	dfsm/dfsm-ast.h \
	dfsm/dfsm-utils.h \
	$(NULL)
dfsm_sources = \
	dfsm/dfsm-bison.y \
	dfsm/dfsm-flex.l \
	dfsm/dfsm-ast.c \
	dfsm/dfsm-parser.c \
	dfsm/dfsm-parser.h \
	$(NULL)

dfsmincludedir = $(pkgincludedir)/dfsm
dfsminclude_HEADERS = \
	$(dfsm_main_header) \
	$(dfsm_public_headers) \
	$(NULL)

dfsm_libdfsm_la_SOURCES = \
	$(dfsm_sources) \
	$(NULL)

dfsm_libdfsm_la_YFLAGS = \
	--warnings=all \
	-d \
	--report=all \
	$(NULL)

dfsm_libdfsm_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/dfsm \
	-DG_LOG_DOMAIN=\"libdfsm\" \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS) \
	$(NULL)

dfsm_libdfsm_la_CFLAGS = \
	$(WARN_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

dfsm_libdfsm_la_LIBADD = \
	$(GLIB_LIBS) \
	$(AM_LIBADD) \
	$(NULL)

dfsm_libdfsm_la_LDFLAGS = \
	-version-info $(DFSM_LT_VERSION) \
	-export-symbols $(srcdir)/dfsm/dfsm.symbols \
	-no-undefined \
	$(AM_LDFLAGS) \
	$(NULL)

EXTRA_DIST += dfsm/dfsm.symbols

CLEANFILES += \
	dfsm/dfsm-flex.c \
	dfsm/dfsm_libdfsm_la-dfsm-bison.c \
	dfsm/dfsm_libdfsm_la-dfsm-bison.h \
	dfsm/dfsm_libdfsm_la-dfsm-bison.output \
	$(NULL)

# libdfsm tests
test_cppflags = \
	-I$(top_srcdir) \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS) \
	$(NULL)

test_cflags = \
	$(WARN_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

test_ldadd = \
	$(top_builddir)/dfsm/libdfsm.la \
	$(GLIB_LIBS) \
	$(NULL)

noinst_PROGRAMS += dfsm/tests/ast

dfsm_tests_ast_SOURCES = dfsm/tests/ast.c
dfsm_tests_ast_CPPFLAGS = $(test_cppflags)
dfsm_tests_ast_CFLAGS = $(test_cflags)
dfsm_tests_ast_LDADD = $(test_ldadd)

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
