CC            = gcc
RANLIB        = ranlib
AR            = ar
STRIP         = strip
ARFLAGS       = cr
LDFLAGS       =
CFLAGS        = -O2 -Wall -Wpointer-arith -Wno-char-subscripts
INCDIR        = -I. -I.. -I../build -I../lib -I../misc -I../popt -I../rpmdb -I../rpmio
RPMDIR        = ..
DESTDIR       = /usr/local

XMLBUILD      = rpmxmlbuild
XMLBUILD_SRC  = rpmxmlbuild.c
XMLBUILD_OBJ  = $(XMLBUILD_SRC:.c=.o)

XMLLIB        = libxmlrpm.a
XMLLIB_SRC    = xml2rpm.c xmlbuild.c xmlmisc.c xmlparse.c xmlstruct.c xmlverify.c
XMLLIB_H      = $(XMLLIB_SRC:.c=.h)
XMLLIB_OBJ    = $(XMLLIB_SRC:.c=.o)

LIBS          = $(XMLLIB) $(RPMDIR)/build/.libs/librpmbuild.a \
		$(RPMDIR)/lib/.libs/librpm.a $(RPMDIR)/rpmdb/.libs/librpmdb.a \
		$(RPMDIR)/rpmio/.libs/librpmio.a $(RPMDIR)/popt/.libs/libpopt.a \
		-lz -lexpat -lbz2
LIBDIR        = -L. -L$(RPMDIR)/.libs -L/usr/lib

all: $(XMLLIB) $(XMLBUILD)

.c.o:
	$(CC) $(CFLAGS) $(INCDIR) -c $?

strip:
	$(STRIP) $(XMLBUILD)

install:
	@(cp $(XMLBUILD) $(DESTDIR)/bin)
	@(cp $(XMLLIB) $(DESTDIR)/lib)
	@(cp $(XMLLIB_H) $(DESTDIR)/include)

$(XMLLIB): $(XMLLIB_OBJ)
	$(AR) $(ARFLAGS) $(XMLLIB) $(XMLLIB_OBJ)
	$(RANLIB) $(XMLLIB)

$(XMLBUILD): $(XMLLIB) $(XMLBUILD_OBJ)
	$(CC) $(LDFLAGS) $(CFLAGS) $(INCDIR) -o $(XMLBUILD) $(XMLBUILD_OBJ) $(LIBS) $(LIBDIR)

clean:
	rm -rf $(XMLLIB) $(XMLBUILD) *.o core
