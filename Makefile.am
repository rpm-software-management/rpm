# Top level Makefile for rpm

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = CHANGES CREDITS GROUPS README.amiga INSTALL \
	RPM-GPG-KEY RPM-PGP-KEY \
	autodeps/none autodeps/*.prov autodeps/*.req autogen.sh \
	gendiff installplatform platform* \
	po/*.in po/*.po po/rpm.pot \
	python/*.am python/*.in python/*.c python/*.h python/testit \
	rpm.magic rpmpopt-$(VERSION) rpmqv.c rpm.spec

# XXX TODO: python perl
SUBDIRS = intl po popt rpmio lib build misc tools scripts tests doc .

INCLUDES = -I$(top_srcdir)/build -I$(top_srcdir)/lib -I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/popt @INCPATH@

LIBS =

myLDFLAGS = # -L$(top_builddir)/build/.libs -L$(top_builddir)/lib/.libs \
	-L$(top_builddir)/rpmio/.libs -L$(top_builddir)/popt/.libs

# XXX libtool can/should generate dependent libs.
# XXX solaris2.6 cannot use *.la with --all-static (downrev binutils/egcs?)
# XXX top_builddir ldpaths are wrong building in tree
myLDADD = $(top_builddir)/build/librpmbuild.la $(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmio/librpmio.la $(top_builddir)/popt/libpopt.la \
	@INTLLIBS@

rpmbindir = `echo $(bindir) | sed -e s,usr/bin,bin,`
rpmbin_PROGRAMS = rpm

bin_PROGRAMS =		rpm2cpio
bin_SCRIPTS =		gendiff

pkglibdir =		@RPMCONFIGDIR@
pkglib_PROGRAMS =	rpmb rpmdb rpmi rpmk rpmq
pkglib_DATA =		rpmrc rpmpopt-$(VERSION) macros
pkglib_SCRIPTS =	find-provides find-requires mkinstalldirs \
			config.guess config.sub

rpmpopt-$(VERSION): rpmpopt
	cp $< $@

noinst_HEADERS = \
	acconfig.h	build.h		system.h

rpm_SOURCES =		rpm.c
rpm_LDFLAGS =		@LDFLAGS_STATIC@ $(myLDFLAGS)
rpm_LDADD =		$(myLDADD) @LIBMISC@

rpmb_SOURCES =		build.c
rpmb_LDFLAGS =		@LDFLAGS_STATIC@ $(myLDFLAGS)
rpmb_LDADD =		rpmb.o $(myLDADD)
rpmb.o:	rpmqv.c
	$(COMPILE) -DIAM_RPMBT -o $@ -c $<

rpmdb_SOURCES =
rpmdb_LDFLAGS =		@LDFLAGS_STATIC@ $(myLDFLAGS)
rpmdb_LDADD =		rpmdb.o $(myLDADD)
rpmdb.o:	rpmqv.c
	$(COMPILE) -DIAM_RPMDB -o $@ -c $<

rpmi_SOURCES =
rpmi_LDFLAGS =		@LDFLAGS_STATIC@ $(myLDFLAGS)
rpmi_LDADD =		rpmi.o $(myLDADD)
rpmi.o:	rpmqv.c
	$(COMPILE) -DIAM_RPMEIU -o $@ -c $<

rpmk_SOURCES =
rpmk_LDFLAGS =		@LDFLAGS_STATIC@ $(myLDFLAGS)
rpmk_LDADD =		rpmk.o $(myLDADD)
rpmk.o:	rpmqv.c
	$(COMPILE) -DIAM_RPMK -o $@ -c $<

rpmq_SOURCES =
rpmq_LDFLAGS =		@LDFLAGS_STATIC@ $(myLDFLAGS)
rpmq_LDADD =		rpmq.o $(myLDADD)
rpmq.o:	rpmqv.c
	$(COMPILE) -DIAM_RPMQV -o $@ -c $<

rpm2cpio_SOURCES =	rpm2cpio.c
rpm2cpio_LDFLAGS =	$(myLDFLAGS)
rpm2cpio_LDADD =	$(myLDADD) @LIBMISC@

$(PROGRAMS): 		$(myLDADD)

.PHONY:	lclint
lclint:
	lclint $(DEFS) $(INCLUDES) $(rpm_SOURCES) $(rpmb_SOURCES) rpmqv.c

CVSTAG = r$(subst .,-,$(VERSION))

install-data-local:
	@$(mkinstalldirs) $(DESTDIR)$(varprefix)/lib/rpm
	@rm -f $(DESTDIR)/$(libdir)/rpmrc
	@@LN_S@ rpm/rpmrc $(DESTDIR)/$(libdir)/rpmrc
	@rm -f $(DESTDIR)/$(libdir)/rpmpopt
	@@LN_S@ rpm/rpmpopt $(DESTDIR)/$(libdir)/rpmpopt
	@rm -f $(DESTDIR)/$(libdir)/rpm/rpmt
	@@LN_S@ rpmb $(DESTDIR)/$(libdir)/rpm/rpmt
	@rm -f $(DESTDIR)/$(libdir)/rpm/rpme
	@@LN_S@ rpmi $(DESTDIR)/$(libdir)/rpm/rpme
	@rm -f $(DESTDIR)/$(libdir)/rpm/rpmu
	@@LN_S@ rpmi $(DESTDIR)/$(libdir)/rpm/rpmu
	@rm -f $(DESTDIR)/$(libdir)/rpm/rpmv
	@@LN_S@ rpmq $(DESTDIR)/$(libdir)/rpm/rpmv
	rm -f $(DESTDIR)/$(bindir)/rpmbuild
	@LN_S@ ../lib/rpm/rpmb $(DESTDIR)/$(bindir)/rpmbuild
	rm -f $(DESTDIR)/$(bindir)/rpmquery
	@LN_S@ ../lib/rpm/rpmq $(DESTDIR)/$(bindir)/rpmquery
	rm -f $(DESTDIR)/$(bindir)/rpmverify
	@LN_S@ ../lib/rpm/rpmv $(DESTDIR)/$(bindir)/rpmverify
	rm -f $(DESTDIR)/$(bindir)/rpmsign
	@LN_S@ ../lib/rpm/rpmk $(DESTDIR)/$(bindir)/rpmsign
	for bf in db e i u ; do \
	    rm -f $(DESTDIR)/$(bindir)/rpm$$bf ; \
	    @LN_S@ ../lib/rpm/rpm$$bf $(DESTDIR)/$(bindir)/rpm$$bf ; \
	done
	@$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/BUILD
	@case "@build_cpu@" in \
	*86)	$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/i386;\
		$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/i486;\
		$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/i586;\
		$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/i686;\
		$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/athlon ;;\
	alpha*) $(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/alpha;\
		$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/alphaev6 ;;\
	sparc*) $(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/sparc;\
		$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/sparcv9;\
		$(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/sparc64 ;;\
	powerpc*) $(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/ppc ;;\
	esac
	@case "@build_os@" in \
	mint) $(mkinstalldirs) $(DESTDIR)/$(prefix)/src/redhat/RPMS/m68kmint ;;\
	solaris*|linux*) \
	   chmod u+x ./installplatform; DESTDIR="$(DESTDIR)" pkglibdir="$(pkglibdir)" ./installplatform rpmrc macros platform ; \
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
