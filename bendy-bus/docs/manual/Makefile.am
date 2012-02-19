include $(top_srcdir)/gnome-doc-utils.make
dist-hook: doc-dist-hook

DOC_ID = bendy-bus
DOC_LINGUAS =
DOC_FIGURES =
DOC_PAGES = \
	language.page \
	$(NULL)

GITIGNOREFILES = \
	$(_DOC_LC_PAGES) \
	$(_DOC_MOFILES) \
	$(NULL)

-include $(top_srcdir)/git.mk
