# Makefile for popt library.

AUTOMAKE_OPTIONS = 1.3 foreign

SUBDIRS = po

INCLUDES = -I$(top_srcdir)

noinst_HEADERS = popt.h
noinst_LIBRARIES = libpopt.a
libpopt_a_SOURCES = popt.c findme.c poptparse.c poptconfig.c popthelp.c
