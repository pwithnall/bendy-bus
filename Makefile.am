SUBDIRS = po

# Setup
bin_PROGRAMS =
dist_bin_SCRIPTS =
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
	dfsm/dfsm-ast-node.h \
	dfsm/dfsm-ast-object.h \
	dfsm/dfsm-ast-expression.h \
	dfsm/dfsm-ast-expression-function-call.h \
	dfsm/dfsm-ast-expression-data-structure.h \
	dfsm/dfsm-ast-expression-unary.h \
	dfsm/dfsm-ast-expression-binary.h \
	dfsm/dfsm-ast-data-structure.h \
	dfsm/dfsm-ast-transition.h \
	dfsm/dfsm-ast-precondition.h \
	dfsm/dfsm-ast-statement.h \
	dfsm/dfsm-ast-statement-assignment.h \
	dfsm/dfsm-ast-statement-throw.h \
	dfsm/dfsm-ast-statement-emit.h \
	dfsm/dfsm-ast-statement-reply.h \
	dfsm/dfsm-ast-variable.h \
	dfsm/dfsm-dbus-output-sequence.h \
	dfsm/dfsm-environment.h \
	dfsm/dfsm-machine.h \
	dfsm/dfsm-object.h \
	dfsm/dfsm-output-sequence.h \
	dfsm/dfsm-parser.h \
	dfsm/dfsm-utils.h \
	$(NULL)
dfsm_sources = \
	dfsm/dfsm-bison.y \
	dfsm/dfsm-flex.l \
	dfsm/dfsm-ast-node.c \
	dfsm/dfsm-ast-object.c \
	dfsm/dfsm-ast-expression.c \
	dfsm/dfsm-ast-expression-function-call.c \
	dfsm/dfsm-ast-expression-data-structure.c \
	dfsm/dfsm-ast-expression-unary.c \
	dfsm/dfsm-ast-expression-binary.c \
	dfsm/dfsm-ast-data-structure.c \
	dfsm/dfsm-ast-transition.c \
	dfsm/dfsm-ast-precondition.c \
	dfsm/dfsm-ast-statement.c \
	dfsm/dfsm-ast-statement-assignment.c \
	dfsm/dfsm-ast-statement-throw.c \
	dfsm/dfsm-ast-statement-emit.c \
	dfsm/dfsm-ast-statement-reply.c \
	dfsm/dfsm-ast-variable.c \
	dfsm/dfsm-dbus-output-sequence.c \
	dfsm/dfsm-parser.c \
	dfsm/dfsm-parser-internal.h \
	dfsm/dfsm-probabilities.c \
	dfsm/dfsm-probabilities.h \
	dfsm/dfsm-machine.c \
	dfsm/dfsm-object.c \
	dfsm/dfsm-output-sequence.c \
	dfsm/dfsm-environment.c \
	dfsm/dfsm-internal.c \
	dfsm/dfsm-internal.h \
	dfsm/dfsm-utils.c \
	$(NULL)

dfsmincludedir = $(pkgincludedir)/dfsm
dfsminclude_HEADERS = \
	$(dfsm_main_header) \
	$(dfsm_public_headers) \
	$(NULL)

dfsm_libdfsm_la_SOURCES = \
	$(dfsm_marshal_sources) \
	$(dfsm_public_headers) \
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
	$(LIBM) \
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

# pkgconfig data
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = dfsm/dfsm.pc

EXTRA_DIST += dfsm/dfsm.pc.in
CLEANFILES += $(pkgconfig_DATA)

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

noinst_PROGRAMS += dfsm/tests/reachability

dfsm_tests_reachability_SOURCES = dfsm/tests/reachability.c
dfsm_tests_reachability_CPPFLAGS = $(test_cppflags)
dfsm_tests_reachability_CFLAGS = $(test_cflags)
dfsm_tests_reachability_LDADD = $(test_ldadd)

GITIGNOREFILES += \
	dfsm/tests/.dirstamp \
	dfsm/tests/.libs/ \
	$(NULL)

# bendy-bus
bin_PROGRAMS += bendy-bus/bendy-bus

bendy_bus_bendy_bus_SOURCES = \
	$(bendy_bus_marshal_sources) \
	bendy-bus/main.c \
	bendy-bus/program-wrapper.c \
	bendy-bus/program-wrapper.h \
	bendy-bus/dbus-daemon.c \
	bendy-bus/dbus-daemon.h \
	bendy-bus/test-program.c \
	bendy-bus/test-program.h \
	bendy-bus/logging.c \
	bendy-bus/logging.h \
	$(NULL)

bendy_bus_bendy_bus_CPPFLAGS = \
	-I$(top_srcdir) \
	-DPACKAGE_LOCALE_DIR=\""$(datadir)/locale"\" \
	-DG_LOG_DOMAIN=\"bendy-bus\" \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS) \
	$(NULL)

bendy_bus_bendy_bus_CFLAGS = \
	$(WARN_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

bendy_bus_bendy_bus_LDADD = \
	$(top_builddir)/dfsm/libdfsm.la \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
	$(AM_LDADD) \
	$(NULL)

# git.mk can't handle non-recursive automake so well
GITIGNOREFILES += \
	bendy-bus/.dirstamp \
	bendy-bus/.libs/ \
	$(NULL)

# bendy-bus-lcov
dist_bin_SCRIPTS += bendy-bus/bendy-bus-lcov

# bendy-bus-lint
bin_PROGRAMS += bendy-bus-lint/bendy-bus-lint

bendy_bus_lint_bendy_bus_lint_SOURCES = \
	bendy-bus-lint/main.c \
	$(NULL)

bendy_bus_lint_bendy_bus_lint_CPPFLAGS = \
	-I$(top_srcdir) \
	-DPACKAGE_LOCALE_DIR=\""$(datadir)/locale"\" \
	-DG_LOG_DOMAIN=\"bendy-bus-lint\" \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS) \
	$(NULL)

bendy_bus_lint_bendy_bus_lint_CFLAGS = \
	$(WARN_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

bendy_bus_lint_bendy_bus_lint_LDADD = \
	$(top_builddir)/dfsm/libdfsm.la \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
	$(AM_LDADD) \
	$(NULL)

# git.mk can't handle non-recursive automake so well
GITIGNOREFILES += \
	bendy-bus-lint/.dirstamp \
	bendy-bus-lint/.libs/ \
	$(NULL)

# bendy-bus-viz
bin_PROGRAMS += bendy-bus-viz/bendy-bus-viz

bendy_bus_viz_bendy_bus_viz_SOURCES = \
	bendy-bus-viz/main.c \
	$(NULL)

bendy_bus_viz_bendy_bus_viz_CPPFLAGS = \
	-I$(top_srcdir) \
	-DPACKAGE_LOCALE_DIR=\""$(datadir)/locale"\" \
	-DG_LOG_DOMAIN=\"bendy-bus-viz\" \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS) \
	$(NULL)

bendy_bus_viz_bendy_bus_viz_CFLAGS = \
	$(WARN_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

bendy_bus_viz_bendy_bus_viz_LDADD = \
	$(top_builddir)/dfsm/libdfsm.la \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
	$(AM_LDADD) \
	$(NULL)

# git.mk can't handle non-recursive automake so well
GITIGNOREFILES += \
	bendy-bus-viz/.dirstamp \
	bendy-bus-viz/.libs/ \
	$(NULL)

# Marshalling
bendy_bus_marshal_sources = \
	bendy-bus/marshal.c \
	bendy-bus/marshal.h \
	$(NULL)

bendy-bus/marshal.h: $(top_srcdir)/bendy-bus/marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=dsim_marshal --header $< > $@
bendy-bus/marshal.c: $(top_srcdir)/bendy-bus/marshal.list bendy-bus/marshal.h
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=dsim_marshal --header --body $< > $@

EXTRA_DIST += $(top_srcdir)/bendy-bus/marshal.list
CLEANFILES += $(bendy_bus_marshal_sources)

# Example machines
EXTRA_DIST += \
	machines/telepathy-cm.machine \
	machines/telepathy-cm.xml \
	$(NULL)

# Version header (see git-version-gen)
EXTRA_DIST += git-version-gen

BUILT_SOURCES += $(top_srcdir)/.version

$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

GITIGNOREFILES += \
	.tarball-version \
	.version \
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
