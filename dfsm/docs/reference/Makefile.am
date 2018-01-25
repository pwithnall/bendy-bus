# gtk-doc configuration

DOC_MODULE = dfsm
DOC_MAIN_SGML_FILE = $(DOC_MODULE)-docs.xml
DOC_SOURCE_DIR = "$(abs_top_srcdir)/dfsm"
SCANGOBJ_OPTIONS =
SCAN_OPTIONS = --rebuild-types
MKDB_OPTIONS = --sgml-mode --output-format=xml --name-space=dfsm
FIXXREF_OPTIONS =
MKHTML_OPTIONS = --path="$(abs_builddir)"

HFILE_GLOB = $(top_srcdir)/dfsm/*.h
CFILE_GLOB = $(top_srcdir)/dfsm/*.c

IGNORE_HFILES = \
	dfsm-internal.h \
	dfsm-marshal.h \
	dfsm-parser-internal.h \
	dfsm-probabilities.h \
	test-output-sequence.h \
	test-utils.h \
	$(NULL)

HTML_IMAGES =
content_files =
expand_content_files =

AM_CPPFLAGS = -I$(top_srcdir) $(GLIB_CFLAGS)
GTKDOC_LIBS = $(top_builddir)/dfsm/libdfsm.la

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

# Fixes for gtk-doc misbehaviour are below:

# This should be in gtk-doc.make, since we are calling gtkdoc-scan with --rebuild-types
SCANOBJ_FILES += $(DOC_MODULE).types

distclean-local: distclean-docs
distclean-docs:
	cd $(srcdir) && \
	  rm -rf xml $(REPORT_FILES) \
	         $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt

maintainer-clean-local: maintainer-clean-docs
maintainer-clean-docs: clean
	cd $(srcdir) && rm -rf xml html

# Checking
# TODO: Re-enable gtk-doc tests once the documentation is fixed
#if ENABLE_GTK_DOC
#TESTS_ENVIRONMENT = cd $(builddir) &&
#TESTS = $(GTKDOC_CHECK)
#endif

-include $(top_srcdir)/git.mk
