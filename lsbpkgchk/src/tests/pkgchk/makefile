LSBPKGCHK_VERSION=unofficial
CFLAGS=-g -DLSBVERSION=\"$$LSBVERSION\" -DLSBPKGCHK_VERSION=\"$(LSBPKGCHK_VERSION)\" -Wall -I../rpmchk

OBJS=pkgchk.o
LIB=../rpmchk/librpmchk.a ../tetj/libtetj.a

all: pkgchk

pkgchk: $(OBJS) $(LIB)
	$(CC) $(CFLAGS) -o pkgchk $(OBJS) $(LIB) -lz

install:
	install -d $(INSTALL_ROOT)/$(BINDIR)
	install pkgchk $(INSTALL_ROOT)/$(BINDIR)/lsbpkgchk
	install -d $(INSTALL_ROOT)/$(MANDIR)/man1
	install lsbpkgchk.1 $(INSTALL_ROOT)/$(MANDIR)/man1/lsbpkgchk.1

clean:
	rm -f $(OBJS) pkgchk

distclean: clean

test_journal: test_journal.o tetj.o
	$(CC) $(CFLAGS) -o test_journal test_journal.o tetj.o
