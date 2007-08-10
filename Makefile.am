# Top level Makefile for rpm

LINT = splint

EXTRA_DIST = CHANGES ChangeLog CREDITS Doxyheader GROUPS INSTALL \
	autodeps autogen.sh \
	config.site db db3/configure installplatform platform* \
	rpm.magic rpmpopt-$(VERSION)

SUBDIRS = po misc @WITH_ZLIB_SUBDIR@ @WITH_DB_SUBDIR@ @WITH_SQLITE3_SUBDIR@ lua rpmio rpmdb lib build python tools scripts doc .

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = rpm.pc

AM_CPPFLAGS = \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	@WITH_BEECRYPT_INCLUDE@ \
	@WITH_POPT_INCLUDE@ \
	-I$(top_srcdir)/misc \
	@WITH_LIBELF_INCLUDE@ \
	@INCPATH@

staticLDFLAGS = @LDFLAGS_STATIC@ @LDFLAGS_NPTL@

myLDFLAGS = @WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@

myLDADD = \
	lib/librpm.la \
	rpmdb/librpmdb.la \
	rpmio/librpmio.la \
	@WITH_POPT_LIB@ \
	@WITH_ZLIB_LIB@ \
	@LIBMISC@

rpmbindir = `echo $(bindir) | sed -e s,usr/bin,bin,`
rpmbin_PROGRAMS = rpm

bin_PROGRAMS =		rpm2cpio

pkglibdir =		@RPMCONFIGDIR@
pkglib_PROGRAMS =	rpmb rpmd rpmi rpmk rpmq
pkglib_DATA =		rpmrc rpmpopt-$(VERSION) macros
pkglib_SCRIPTS =	find-provides find-requires mkinstalldirs \
			config.guess config.sub config.site

rpmpopt-$(VERSION): rpmpopt
	cp rpmpopt $@

rpm_SOURCES =		rpmqv.c debug.h system.h
rpm_CPPFLAGS =		$(AM_CPPFLAGS) -DIAM_RPMDB -DIAM_RPMEIU -DIAM_RPMK -DIAM_RPMQV
rpm_LDFLAGS =		$(myLDFLAGS)
rpm_LDADD =		build/.libs/librpmbuild.a $(myLDADD)

rpmb_SOURCES =		build.c rpmqv.c build.h debug.h system.h
rpmb_CPPFLAGS =		$(AM_CPPFLAGS) -DIAM_RPMBT
rpmb_LDFLAGS =		$(myLDFLAGS)
rpmb_LDADD =		build/librpmbuild.la $(myLDADD)

rpmd_SOURCES =		rpmqv.c debug.h system.h
rpmd_CPPFLAGS =		$(AM_CPPFLAGS) -DIAM_RPMDB
rpmd_LDFLAGS =		$(myLDFLAGS)
rpmd_LDADD =		$(myLDADD)

rpmi_SOURCES =		rpmqv.c debug.h system.h
rpmi_CPPFLAGS =		$(AM_CPPFLAGS) -DIAM_RPMEIU
#rpmi_LDFLAGS =		$(myLDFLAGS) $(staticLDFLAGS)
#rpmi_LDADD =		rpmi.o $(myLDADD) @WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@
rpmi_LDFLAGS =		$(myLDFLAGS)
rpmi_LDADD =		$(myLDADD)

rpmk_SOURCES =		rpmqv.c debug.h system.h
rpmk_CPPFLAGS =		$(AM_CPPFLAGS) -DIAM_RPMK
rpmk_LDFLAGS =		$(myLDFLAGS)
rpmk_LDADD =		$(myLDADD)

rpmq_SOURCES =		rpmqv.c debug.h system.h
rpmq_CPPFLAGS =		$(AM_CPPFLAGS) -DIAM_RPMQV
rpmq_LDFLAGS =		$(myLDFLAGS)
rpmq_LDADD =		build/librpmbuild.la $(myLDADD)

rpm2cpio_SOURCES =	rpm2cpio.c debug.h system.h
rpm2cpio_LDFLAGS =	$(myLDFLAGS)
rpm2cpio_LDADD =	$(myLDADD) @LIBMISC@

.PHONY:	splint
splint:
	splint \
	    -load build/rpmbuild.lcd \
	    -load lib/rpmlib.lcd \
	    -load rpmdb/rpmdb.lcd \
	    -load rpmio/rpmio.lcd \
		$(DEFS) $(AM_CPPFLAGS) rpmqv.c $(rpmb_SOURCES)

.PHONY:	lint
lint:
	$(LINT) -Dlint $(DEFS) $(AM_CPPFLAGS) rpmqv.c $(rpmb_SOURCES) \
		`make -s sources -C build` \
		`make -s sources -C lib` \
		`make -s sources -C rpmdb` \
		`make -s sources -C rpmio` 

CVSTAG = r$(subst .,-,$(VERSION))

pkgsrcdir = $(prefix)/src/$(RPMCANONVENDOR)

install-data-local:
	@$(MKDIR_P) $(DESTDIR)$(varprefix)/lib/rpm
	@rm -f $(DESTDIR)$(pkglibdir)/rpmt
	@@LN_S@ rpmb $(DESTDIR)$(pkglibdir)/rpmt
	@rm -f $(DESTDIR)$(pkglibdir)/rpme
	@@LN_S@ rpmi $(DESTDIR)$(pkglibdir)/rpme
	@rm -f $(DESTDIR)$(pkglibdir)/rpmu
	@@LN_S@ rpmi $(DESTDIR)$(pkglibdir)/rpmu
	@rm -f $(DESTDIR)$(pkglibdir)/rpmv
	@@LN_S@ rpmq $(DESTDIR)$(pkglibdir)/rpmv
	rm -f $(DESTDIR)$(bindir)/rpmbuild
	@LN_S@ ../lib/rpm/rpmb $(DESTDIR)$(bindir)/rpmbuild
	rm -f $(DESTDIR)$(bindir)/rpmquery
	@LN_S@ ../lib/rpm/rpmq $(DESTDIR)$(bindir)/rpmquery
	rm -f $(DESTDIR)$(bindir)/rpmverify
	@LN_S@ ../lib/rpm/rpmv $(DESTDIR)$(bindir)/rpmverify
	rm -f $(DESTDIR)$(bindir)/rpmsign
	@LN_S@ ../lib/rpm/rpmk $(DESTDIR)$(bindir)/rpmsign
	rm -f $(DESTDIR)$(bindir)/rpmdb ; \
	@LN_S@ ../lib/rpm/rpmd $(DESTDIR)$(bindir)/rpmdb ; \
	for bf in e i u ; do \
	    rm -f $(DESTDIR)$(bindir)/rpm$$bf ; \
	    @LN_S@ ../lib/rpm/rpm$$bf $(DESTDIR)$(bindir)/rpm$$bf ; \
	done
	@for dir in BUILD RPMS SOURCES SPECS SRPMS ; do\
	    $(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/$$dir;\
	done
	@case "@host_cpu@" in \
	*86)	$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/i386 ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/i486 ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/i586 ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/i686 ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/athlon ;;\
	alpha*) $(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/alpha ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/alphaev6 ;;\
	arm*)	$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/armv3l ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/armv4l ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/armv4tl ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/armv5tel ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/armv5tejl ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/armv6l ;;\
	sparc*) $(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/sparc ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/sparcv8 ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/sparcv9 ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/sparc64 ;;\
	ia64*)	$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/ia64 ;;\
	s390*)	$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/s390 ;;\
	mipsel*) $(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/mipsel ;;\
	mips*)  $(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/mips ;;\
	powerpc*) $(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/ppc ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/ppciseries ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/ppcpseries ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/ppc64 ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/ppc64iseries ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/ppc64pseries ;;\
	*)	$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/@host_cpu@ ;;\
	esac
	$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/noarch
	@case "@host_os@" in \
	mint) $(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/m68kmint ;;\
	solaris*|linux*|darwin*) \
	   chmod u+x $(top_srcdir)/installplatform; DESTDIR="$(DESTDIR)" pkglibdir="$(pkglibdir)" $(top_srcdir)/installplatform rpmrc macros platform ;; \
	esac
	@$(MKDIR_P) $(DESTDIR)/var/tmp

# XXX to appease distcheck we need to remove "stuff" here...
uninstall-local:
	@rm -rf $(DESTDIR)/$(pkglibdir)/*-*
	
distclean-local:
	@rm -rf Doxytags apidocs 
	@rm -f stamp-h.in db3/*.orig db3/db3lobjs
	@rm -f ChangeLog rpmpopt-$(VERSION)

.PHONY:	setperms
setperms:
	@for f in $(rpmbin_PROGRAMS) ; do\
	    $(__CHOWN) ${RPMUSER}.${RPMGROUP} $(DESTDIR)$(rpmbindir)/$$f ;\
	    $(__CHMOD) g+s $(DESTDIR)$(rpmbindir)/$$f ;\
	done
	@for f in $(bin_PROGRAMS) ; do\
	    $(__CHOWN) ${RPMUSER}.${RPMGROUP} $(DESTDIR)$(bindir)/$$f ;\
	done
	@for f in $(pkglib_PROGRAMS) ; do\
	    $(__CHOWN) ${RPMUSER}.${RPMGROUP} $(DESTDIR)$(pkglibdir)/$$f ;\
	    $(__CHMOD) g+s $(DESTDIR)$(pkglibdir)/$$f ;\
	done
	@for f in $(pkglib_SCRIPTS) ; do\
	    $(__CHOWN) ${RPMUSER}.${RPMGROUP} $(DESTDIR)$(pkglibdir)/$$f ;\
	done
	@$(__CHOWN) ${RPMUSER}.${RPMGROUP} $(DESTDIR)$(pkglibdir)
	@$(__CHOWN) -R ${RPMUSER}.${RPMGROUP} $(DESTDIR)$(varprefix)/lib/rpm
	-@$(__CHMOD) 0664 $(DESTDIR)$(varprefix)/lib/rpm/[A-Z]*
	-@$(__CHMOD) 0775 $(DESTDIR)$(varprefix)/lib/rpm
	-@$(__CHMOD) 0664 $(DESTDIR)$(varprefix)/lib/rpm/__db.*

.PHONY:	unsetgid
unsetgid:
	@for f in $(rpmbin_PROGRAMS) ; do\
	    $(__CHMOD) g-s $(DESTDIR)$(rpmbindir)/$$f ;\
	done
	@for f in $(pkglib_PROGRAMS) ; do\
	    $(__CHMOD) g-s $(DESTDIR)$(pkglibdir)/$$f ;\
	done

.PHONY: tar
tar:
	rm -rf /tmp/rpm-$(VERSION)
	$(MAKE) DESTDIR=/tmp/rpm-$(VERSION) install
	cd /tmp/rpm-$(VERSION) ; tar cvf /tmp/rpm-$(VERSION).tar .

.PHONY: noconfig
noconfig:
	find . -name "Makefile" -exec rm {} \; 
	rm -f *gz *rpm config.*

.PHONY:	doxygen
doxygen @WITH_APIDOCS_TARGET@: Doxyfile rpmpopt-@VERSION@
	rm -rf $@
	$(MKDIR_P) $@
	- [ X"@__DOXYGEN@" != Xno ] && @__DOXYGEN@

ctags:
	find . -type f -name "*.[ch]*" | xargs @CTAGS@

cscope:
	@CSCOPE@ -b -R

cref: ctags cscope

.PHONY: ChangeLog
ChangeLog:
	hg log -v > ChangeLog

ACLOCAL_AMFLAGS = -I m4
