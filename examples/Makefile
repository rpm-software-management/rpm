# Makes the Maxumum RPM example programs
# Scott Bronson, 20 Mar 1999

# NOTE: if you do not have libdb1, change this makefile to use libdb
# (or whatever database library rpm was compiled with).


all: dumprpm showdb showdb2

dumprpm: dumprpm.c
	gcc -Wall dumprpm.c -lrpm -lintl -lgz -o dumprpm

showdb: showdb.c
	gcc -Wall showdb.c -lrpm -lintl -ldb1 -lgz -o showdb

showdb2: showdb2.c
	gcc -Wall showdb2.c -lrpm -lintl -ldb1 -lgz -o showdb2

clean:
	rm -f dumprpm showdb showdb2

