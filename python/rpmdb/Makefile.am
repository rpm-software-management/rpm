# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

PYVER= @WITH_PYTHON_VERSION@

pylibdir = $(shell python -c 'import sys; print sys.path[1]')

EXTRA_DIST = \
	__init__.py dbobj.py db.py dbrecio.py dbshelve.py dbtables.py dbutils.py

rpmdbdir = $(pylibdir)/site-packages/rpmdb
rpmdb_SCRIPTS = \
	__init__.py dbobj.py db.py dbrecio.py dbshelve.py dbtables.py dbutils.py
