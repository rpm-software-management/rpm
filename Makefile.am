# Top level Makefile for rpm

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = CHANGES CREDITS GROUPS README.amiga INSTALL \
	RPM-GPG-KEY RPM-PGP-KEY \
	autodeps/none autodeps/*.prov autodeps/*.req autogen.sh \
	gendiff po/*.in po/*.po po/rpm.pot \
	python/*.am python/*.in python/*.c python/*.h python/testit \
	rpm.magic rpmpopt rpm.spec installplatform

EXTRA_PROGRAMS = rpmconvert

# XXX TODO: python perl
SUBDIRS = intl po popt build lib misc tools scripts tests doc .

INCLUDES = -I$(top_srcdir)/build -I$(top_srcdir)/lib -I$(top_srcdir)/popt \
	@INCPATH@

myldflags= -L$(top_builddir)/build -L$(top_builddir)/lib -L$(top_builddir)/popt

# XXX libtool can/should generate dependent libs.
# XXX solaris2.6 cannot use *.la with --all-static (downrev binutils/egcs?)
myldadd= \
	$(top_builddir)/build/.libs/librpmbuild.a \
	$(top_builddir)/lib/.libs/librpm.a \
	$(top_builddir)/popt/.libs/libpopt.a \
	@INTLLIBS@ @LIBMISC@

LDFLAGS = @LDFLAGS_STATIC@ $(myldflags)
LDADD = $(myldadd)

rpmbindir = `echo $(bindir) | sed -e s,usr/bin,bin,`
rpmbin_PROGRAMS = rpm

bin_PROGRAMS =		rpm2cpio
bin_SCRIPTS =		gendiff

noinst_PROGRAMS =	@BUILD_RPMCONVERT@

pkglibdir = @RPMCONFIGDIR@
pkglib_DATA = rpmrc rpmpopt macros
pkglib_SCRIPTS = find-provides find-requires mkinstalldirs \
	config.guess config.sub

noinst_HEADERS = \
	acconfig.h	build.h		system.h

rpm_SOURCES =		build.c rpm.c

$(PROGRAMS): 		$(myldadd)

.PHONY:	lclint
lclint:
	lclint ${DEFS} ${INCLUDES} ${rpm_SOURCES}

CVSTAG = r$(subst .,-,$(VERSION))

rpm2cpio_SOURCES =	rpm2cpio.c

rpmconvert_SOURCES =	convertdb.c oldrpmdb.c
## Libmisc.a is required for rpmconvert.
rpmconvert_LDADD =	$(myldadd) -lgdbm

install-data-local:
	@$(mkinstalldirs) $(DESTDIR)$(varprefix)/lib/rpm
	@rm -f $(DESTDIR)/$(libdir)/rpmrc
	@ln -s rpm/rpmrc $(DESTDIR)/$(libdir)/rpmrc
	@rm -f $(DESTDIR)/$(libdir)/rpmpopt
	@ln -s rpm/rpmpopt $(DESTDIR)/$(libdir)/rpmpopt
	@$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/BUILD
	@case "@build_cpu@" in \
	*86) $(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/i386 ;;\
	alpha*) $(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/alpha ;;\
	sparc*) $(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/sparc;\
		$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/sparc64 ;;\
	powerpc*) $(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/ppc ;;\
	esac
	@case "@build_os@" in \
	mint) $(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/m68kmint ;;\
	linux*) chmod u+x ./installplatform; DESTDIR="$(DESTDIR)" pkglibdir="$(pkglibdir)" ./installplatform rpmrc macros platform ; \
	   $(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/@build_cpu@ ;;\
	*) $(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/@build_cpu@ ;;\
	esac
	@$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/noarch
	@$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/SOURCES
	@$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/SPECS
	@$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/SRPMS
	@$(mkinstalldirs) $(DESTDIR)/var/tmp

.PHONY: tar
tar:
	rm -rf /tmp/rpm-$(VERSION)
	$(MAKE) DESTDIR=/tmp/rpm-$(VERSION) install
	cd /tmp/rpm-$(VERSION) ; tar cvf /tmp/rpm-$(VERSION).tar .

.PHONY: noconfig
noconfig:
	find . -name "Makefile" -exec rm {} \; 
	rm -f *gz *rpm config.*

.PHONY: archive
archive: 
	@cvs -d `cat CVS/Root` diff 2>&1 > /dev/null || { \
	    echo " " ; \
	    echo "==> Please resolve differences between the repository and" ; \
	    echo "==> your rpm check out before tagging." ; \
	    echo " " ; \
	    cvs -n up ; \
	    echo " " ; \
	    exit 1 ; \
	}
	@cvs -d `cat CVS/Root` -Q tag -F $(CVSTAG) .
	@make dist
	@echo " "
	@echo "The final archive is ./rpm-$(VERSION).tar.gz."

.PHONY:	nextsrpm
nextsrpm: all archive
	@sudo ./rpm -ta rpm-$(VERSION).tar.gz

.PHONY: doxygen
doxygen: Doxyfile
	rm -rf doxygen
	mkdir -p doxygen
	doxygen
