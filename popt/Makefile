LIBOBJECTS = 	popt.o

DEFCFLAGS=-O2

SOURCES =$(subst .o,.c,$(LIBOBJECTS))
LIBPOPT = libpopt.a
INSTALL= @INSTALL@
INSTALL_PROGRAM= @INSTALL_PROGRAM@
INSTALL_DATA= @INSTALL_DATA@

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

ifeq (.depend,$(wildcard .depend))
include .depend
endif
