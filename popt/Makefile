LIBOBJECTS = 	popt.o

DEFCFLAGS=-O2 -Wall

SOURCES =$(subst .o,.c,$(LIBOBJECTS))
LIBPOPT = libpopt.a
LIBS=/usr/lib
INCLUDE=/usr/include

ifeq (../Makefile.inc,$(wildcard ../Makefile.inc))
include ../Makefile.inc
endif

# -----------------------------------------------------------------------

ifeq ($(CFLAGS),)
CFLAGS=$(DEFCFLAGS)
endif

ifeq ($(RANLIB),)
RANLIB=ranlib
endif

ifeq (.depend,$(wildcard .depend))
TARGET=allprogs
else
TARGET=depend allprogs
endif

$(LIBPOPT): $(LIBPOPT)($(LIBOBJECTS))
	$(RANLIB) $@

distclean: clean

clean:
	rm -f *.a *.o *~ $(PROGS) test.out tagtable.c

squeaky: clean
	rm -f .depend

depend:
	$(CPP) $(CFLAGS) -M $(SOURCES) > .depend

install:
	mkdir -p $(PREFIX)/$(INCLUDE)
	mkdir -p $(PREFIX)/$(LIBS)
	install -m 644 popt.h $(PREFIX)/$(INCLUDE)/popt.h
	install -m 644 $(LIBPOPT) $(PREFIX)/$(LIBS)/$(LIBPOPT)

ifeq (.depend,$(wildcard .depend))
include .depend
endif
