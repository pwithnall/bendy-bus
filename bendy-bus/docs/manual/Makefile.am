@YELP_HELP_RULES@

HELP_ID = bendy-bus
HELP_FILES = \
	bendy-bus.page \
	bendy-bus-lcov.page \
	bendy-bus-lint.page \
	bendy-bus-viz.page \
	data-structures.page \
	expressions.page \
	functions.page \
	index.page \
	language.page \
	objects.page \
	preconditions.page \
	simulator.page \
	statements.page \
	structure.page \
	transitions.page \
	$(NULL)

GITIGNOREFILES = \
	html/ \
	xhtml/ \
	$(NULL)

html:
	rm -rf $(builddir)/html && mkdir $(builddir)/html && \
	yelp-build html -o $(builddir)/html $(addprefix C/,$(DOC_PAGES))

xhtml:
	rm -rf $(builddir)/xhtml && mkdir $(builddir)/xhtml && \
	yelp-build html -o $(builddir)/xhtml $(addprefix C/,$(DOC_PAGES))

maintainer-clean-local: maintainer-clean-manual
maintainer-clean-manual: clean
	rm -rf $(builddir)/html $(builddir)/xhtml

.PHONY: html xhtml maintainer-clean-manual

-include $(top_srcdir)/git.mk
