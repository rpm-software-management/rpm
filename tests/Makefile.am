## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST =
SUBDIRS = hello-test

check-recursive: ./usr/ ./bin ./var

./usr ./bin ./var: ../rpm
	make -C .. DESTDIR=`pwd` install
	cp rpmrc macros ./$(pkglibdir)
	rm -f ./@GZIPBIN@
	ln -s @GZIPBIN@ ./@GZIPBIN@
	rm -f ./@BZIP2BIN@
	ln -s @BZIP2BIN@ ./@BZIP2BIN@

clean-local:
	rm -rf ./usr ./bin ./var
