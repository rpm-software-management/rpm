## Process this file with automake to produce Makefile.in.

EXTRA_DIST =
SUBDIRS = hello-test

check-recursive: ./usr/ ./bin ./var

./usr ./bin ./var: ../rpm
	make -C .. DESTDIR=`pwd` install

clean-local:
	rm -rf ./usr ./bin ./var
