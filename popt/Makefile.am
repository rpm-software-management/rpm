# Makefile for popt library.

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = autogen.sh CHANGES $(man_MANS) popt.spec \
	testit.sh test-poptrc \
	po/*.in po/*.po po/popt.pot \
	popt.ps

SUBDIRS = intl po

INCLUDES = -I. -I$(top_srcdir)

noinst_HEADERS = findme.h poptint.h system.h

noinst_PROGRAMS = test1 test2
test1_SOURCES = test1.c
test1_LDFLAGS = -all-static
test1_LDADD = $(lib_LTLIBRARIES)
test2_SOURCES = test2.c
test2_LDFLAGS = -all-static
test2_LDADD = $(lib_LTLIBRARIES)

noinst_SCRIPTS = testit.sh

TESTS_ENVIRONMENT = \
test1="./test1"

TESTS = testit.sh

include_HEADERS = popt.h
lib_LTLIBRARIES = libpopt.la
libpopt_la_SOURCES = popt.c findme.c poptparse.c poptconfig.c popthelp.c

man_MANS = popt.3

.PHONY:	sources
sources:
	@echo $(libpopt_la_SOURCES:%=popt/%)

.PHONY: lclint
lclint:
	lclint ${DEFS} ${INCLUDES} ${libpopt_la_SOURCES}

CVSTAG = $(PACKAGE)-$(subst .,_,$(VERSION))

.PHONY: archive
archive: 
	@echo "This is $(PACKAGE)-$(VERSION)."
	@sleep 5
	@cvs -Q tag -F $(CVSTAG) .
	@rm -rf /tmp/$(PACKAGE)-$(VERSION) /tmp/$(PACKAGE)
	@cd /tmp; cvs -Q -d $(CVSROOT) export -r$(CVSTAG) $(PACKAGE) || :
	@mv /tmp/$(PACKAGE) /tmp/$(PACKAGE)-$(VERSION)
	@cd /tmp/$(PACKAGE)-$(VERSION); ./autogen.sh ; make depend; make distclean
	@cd /tmp/$(PACKAGE)-$(VERSION); ./autogen.sh --noconfigure
	@cd /tmp; tar czSpf $(PACKAGE)-$(VERSION).tar.gz $(PACKAGE)-$(VERSION)
	@rm -rf /tmp/$(PACKAGE)-$(VERSION)
	@cp /tmp/$(PACKAGE)-$(VERSION).tar.gz .
	@rm -f /tmp/$(PACKAGE)-$(VERSION).tar.gz 
	@echo " "
	@echo "The final archive is ./$(PACKAGE)-$(VERSION).tar.gz."

.PHONY: doxygen
doxygen: Doxyfile
	rm -rf doxygen
	mkdir -p doxygen
	doxygen
