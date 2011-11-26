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
	dfsm/dfsm-environment.h \
	dfsm/dfsm-machine.h \
	dfsm/dfsm-object.h \
	dfsm/dfsm-utils.h \
	$(NULL)
dfsm_sources = \
	dfsm/dfsm-bison.y \
	dfsm/dfsm-flex.l \
	dfsm/dfsm-ast.c \
	dfsm/dfsm-parser.c \
	dfsm/dfsm-parser.h \
	dfsm/dfsm-machine.c \
	dfsm/dfsm-object.c \
	dfsm/dfsm-environment.c \
	$(NULL)

dfsmincludedir = $(pkgincludedir)/dfsm
dfsminclude_HEADERS = \
	$(dfsm_main_header) \
	$(dfsm_public_headers) \
	$(NULL)

dfsm_libdfsm_la_SOURCES = \
	$(dfsm_marshal_sources) \
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
	$(GIO_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

dfsm_libdfsm_la_LIBADD = \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
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
	dfsm-flex.c \
	dfsm_libdfsm_la-dfsm-bison.c \
	dfsm_libdfsm_la-dfsm-bison.h \
	dfsm_libdfsm_la-dfsm-bison.output \
	$(NULL)

DISTCLEANFILES += \
	ylwrap \
	$(NULL)

# git.mk can't handle non-recursive automake so well
GITIGNOREFILES += \
	dfsm/.dirstamp \
	dfsm/.libs/ \
	$(NULL)

# Marshalling
dfsm_marshal_sources = \
	dfsm/dfsm-marshal.c \
	dfsm/dfsm-marshal.h \
	$(NULL)

dfsm/dfsm-marshal.h: $(top_srcdir)/dfsm/dfsm-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=dfsm_marshal --header $< > $@
dfsm/dfsm-marshal.c: $(top_srcdir)/dfsm/dfsm-marshal.list dfsm/dfsm-marshal.h
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=dfsm_marshal --header --body $< > $@

EXTRA_DIST += $(top_srcdir)/dfsm/dfsm-marshal.list
CLEANFILES += $(dfsm_marshal_sources)

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

GITIGNOREFILES += \
	dfsm/tests/.dirstamp \
	$(NULL)

# dbus-simulator
bin_PROGRAMS += dbus-simulator/dbus-simulator

dbus_simulator_dbus_simulator_SOURCES = \
	$(dbus_simulator_marshal_sources) \
	dbus-simulator/main.c \
	dbus-simulator/program-wrapper.c \
	dbus-simulator/program-wrapper.h \
	dbus-simulator/dbus-daemon.c \
	dbus-simulator/dbus-daemon.h \
	dbus-simulator/test-program.c \
	dbus-simulator/test-program.h \
	$(NULL)

dbus_simulator_dbus_simulator_CPPFLAGS = \
	-I$(top_srcdir) \
	-DPACKAGE_LOCALE_DIR=\""$(datadir)/locale"\" \
	-DG_LOG_DOMAIN=\"dbus-simulator\" \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS) \
	$(NULL)

dbus_simulator_dbus_simulator_CFLAGS = \
	$(WARN_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

dbus_simulator_dbus_simulator_LDADD = \
	$(top_builddir)/dfsm/libdfsm.la \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
	$(AM_LDADD) \
	$(NULL)

# git.mk can't handle non-recursive automake so well
GITIGNOREFILES += \
	dbus-simulator/.dirstamp \
	dbus-simulator/.libs/ \
	$(NULL)

# Marshalling
dbus_simulator_marshal_sources = \
	dbus-simulator/marshal.c \
	dbus-simulator/marshal.h \
	$(NULL)

dbus-simulator/marshal.h: $(top_srcdir)/dbus-simulator/marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=dsim_marshal --header $< > $@
dbus-simulator/marshal.c: $(top_srcdir)/dbus-simulator/marshal.list dbus-simulator/marshal.h
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=dsim_marshal --header --body $< > $@

EXTRA_DIST += $(top_srcdir)/dbus-simulator/marshal.list
CLEANFILES += $(dbus_simulator_marshal_sources)

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
