## Process this file with automake to produce Makefile.in.

all:

noinst_SCRIPTS = initdb showrc test0

TESTS_ENVIRONMENT = rpm="${top_builddir}/rpm --rcfile ${top_builddir}/tests/$(pkglibdir)/rpmrc"

TESTS = initdb showrc test0
