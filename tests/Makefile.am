## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST =
SUBDIRS = hello-test

check-recursive: ./usr/ ./bin ./var

./usr ./bin ./var: ../rpm
	make -C .. DESTDIR=`pwd` install
	cp rpmrc macros ./$(pkglibdir)
	rm -f ./@GZIPBIN@
	if [ ! -d ./`dirname @GZIPBIN@` ] ; then mkdir -p ./`dirname @GZIPBIN@` ; fi
	ln -s @GZIPBIN@ ./@GZIPBIN@
	rm -f ./@BZIP2BIN@
	if [ ! -d ./`dirname @BZIP2BIN@` ] ; then mkdir -p ./`dirname @BZIP2BIN@` ; fi
	ln -s @BZIP2BIN@ ./@BZIP2BIN@

clean-local:
	rm -rf ./usr ./bin ./var
