AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

SUBDIRS=tests

INCLUDES = -I.

noinst_HEADERS =	sexp.h
noinst_PROGRAMS =	sexp

sexp_SOURCES = \
	sexp-basic.c sexp-input.c sexp-main.c sexp-output.c
sexp_LDADD =	-lpopt

.PHONY:	sources
sources:
	@echo $(sexp_SOURCES:%=rpmio/sexp/%)

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(sexp_SOURCES)
