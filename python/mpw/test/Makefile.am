# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

PYVER= @WITH_PYTHON_VERSION@

pylibdir = $(shell python -c 'import sys; print sys.path[1]')
VALGRIND = # valgrind --verbose --leak-check=yes

EXTRA_DIST = \
	test_all.py test_methods.py \
	unittest.py

all:

check:
	$(VALGRIND) python test_all.py verbose
