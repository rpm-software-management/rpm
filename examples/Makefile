# Makes the Maxumum RPM example programs
# Scott Bronson, 20 Mar 1999

# NOTE: if you do not have libdb1, change this makefile to use libdb
# (or whatever database library rpm was compiled with).


CFLAGS=	-O2 -Wall -I/usr/include/rpm
VER =	4.0.4

LDADD=	-lrpm-$(VER) -lrpmdb-$(VER) -lrpmio-$(VER) -lpopt -lz

COMPILE = $(CC) $(CFLAGS)
LINK =	$(CC)

all: dumprpm showdb showdb2

dumprpm: dumprpm.o
	$(LINK) -o $@ $< $(LDADD)

showdb: showdb.o
	$(LINK) -o $@ $< $(LDADD)

showdb2: showdb2.o
	$(LINK) -o $@ $< $(LDADD)

clean:
	rm -f *.o *.lo dumprpm showdb showdb2

dumprpm.o: dumprpm.c
	$(COMPILE) -o $@ -c dumprpm.c

showdb.o: showdb.c
	$(COMPILE) -o $@ -c showdb.c

showdb2.o: showdb2.c
	$(COMPILE) -o $@ -c showdb2.c
