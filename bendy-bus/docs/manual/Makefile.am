include $(top_srcdir)/gnome-doc-utils.make
dist-hook: doc-dist-hook

DOC_ID = bendy-bus
DOC_LINGUAS =
DOC_FIGURES =
DOC_PAGES = \
	data-structures.page \
	expressions.page \
	functions.page \
	index.page \
	language.page \
	objects.page \
	preconditions.page \
	statements.page \
	structure.page \
	transitions.page \
	$(NULL)

GITIGNOREFILES = \
	$(_DOC_LC_PAGES) \
	$(_DOC_MOFILES) \
	$(NULL)

html:
	rm -rf $(builddir)/html && mkdir $(builddir)/html && \
	yelp-build html -o $(builddir)/html $(addprefix C/,$(DOC_PAGES))

xhtml:
	rm -rf $(builddir)/xhtml && mkdir $(builddir)/xhtml && \
	yelp-build html -o $(builddir)/xhtml $(addprefix C/,$(DOC_PAGES))

check: check-yelp
check-yelp:
	$(foreach command,links orphans status validate, \
		yelp-check $(command) $(addprefix C/,$(DOC_PAGES)); \
	)

maintainer-clean-local: maintainer-clean-manual
maintainer-clean-manual: clean
	rm -rf $(builddir)/html $(builddir)/xhtml

.PHONY: html xhtml check-yelp maintainer-clean-manual

-include $(top_srcdir)/git.mk
