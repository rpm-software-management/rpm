# Top level Makefile for rpm

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

EXTRA_DIST = CHANGES CREDITS Doxyheader GROUPS README.amiga INSTALL \
	autodeps/none autodeps/*.prov autodeps/*.req autogen.sh \
	config.site db db3/configure installplatform platform* \
	po/*.in po/*.po po/rpm.pot \
	rpm.magic rpmpopt-$(VERSION) rpmqv.c rpm.c

SUBDIRS = po misc @WITH_ZLIB_SUBDIR@ @WITH_ELFUTILS_SUBDIR@ @WITH_MAGIC_SUBDIR@ @WITH_DB_SUBDIR@ @WITH_SQLITE3_SUBDIR@ @WITH_POPT_SUBDIR@ @WITH_BEECRYPT_SUBDIR@ @WITH_NEON_SUBDIR@ lua rpmio rpmdb lib build @WITH_PYTHON_SUBDIR@ tools scripts tests doc .

INCLUDES = \
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
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
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

noinst_HEADERS = build.h debug.h system.h

rpm_SOURCES =
rpm_LDFLAGS =		$(myLDFLAGS)
rpm_LDADD =		rpm.o $(top_builddir)/build/.libs/librpmbuild.a $(myLDADD)
rpm.o:	$(top_srcdir)/rpmqv.c
	$(COMPILE) -DIAM_RPMDB -DIAM_RPMEIU -DIAM_RPMK -DIAM_RPMQV -o $@ -c $(top_srcdir)/rpmqv.c

rpmb_SOURCES =		$(top_srcdir)/build.c
rpmb_LDFLAGS =		$(myLDFLAGS)
rpmb_LDADD =		rpmb.o $(top_builddir)/build/librpmbuild.la $(myLDADD)
rpmb.o:	$(top_srcdir)/rpmqv.c
	$(COMPILE) -DIAM_RPMBT -o $@ -c $(top_srcdir)/rpmqv.c

rpmd_SOURCES =
rpmd_LDFLAGS =		$(myLDFLAGS)
rpmd_LDADD =		rpmd.o $(myLDADD)
rpmd.o:	$(top_srcdir)/rpmqv.c
	$(COMPILE) -DIAM_RPMDB -o $@ -c $(top_srcdir)/rpmqv.c

rpmi_SOURCES =
#rpmi_LDFLAGS =		$(myLDFLAGS) $(staticLDFLAGS)
#rpmi_LDADD =		rpmi.o $(myLDADD) @WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@
rpmi_LDFLAGS =		$(myLDFLAGS)
rpmi_LDADD =		rpmi.o $(myLDADD)
rpmi.o:	$(top_srcdir)/rpmqv.c
	$(COMPILE) -DIAM_RPMEIU -o $@ -c $(top_srcdir)/rpmqv.c

rpmk_SOURCES =
rpmk_LDFLAGS =		$(myLDFLAGS)
rpmk_LDADD =		rpmk.o $(myLDADD)
rpmk.o:	$(top_srcdir)/rpmqv.c
	$(COMPILE) -DIAM_RPMK -o $@ -c $(top_srcdir)/rpmqv.c

rpmq_SOURCES =
rpmq_LDFLAGS =		$(myLDFLAGS)
rpmq_LDADD =		rpmq.o $(top_builddir)/build/librpmbuild.la $(myLDADD)
rpmq.o:	$(top_srcdir)/rpmqv.c
	$(COMPILE) -DIAM_RPMQV -o $@ -c $(top_srcdir)/rpmqv.c

rpm2cpio_SOURCES =	$(top_srcdir)/rpm2cpio.c
rpm2cpio_LDFLAGS =	$(myLDFLAGS)
rpm2cpio_LDADD =	$(myLDADD) @LIBMISC@

$(PROGRAMS): 		$(myLDADD) @WITH_APIDOCS_TARGET@

.PHONY:	splint
splint:
	splint \
	    -load build/rpmbuild.lcd \
	    -load lib/rpmlib.lcd \
	    -load rpmdb/rpmdb.lcd \
	    -load rpmio/rpmio.lcd \
	    -load popt/popt.lcd \
		$(DEFS) $(INCLUDES) rpmqv.c $(rpmb_SOURCES)

.PHONY:	lint
lint:
	$(LINT) -Dlint $(DEFS) $(INCLUDES) rpmqv.c $(rpmb_SOURCES) \
		`make -s sources -C build` \
		`make -s sources -C lib` \
		`make -s sources -C rpmdb` \
		`make -s sources -C rpmio` \
		`make -s sources -C beecrypt` \
		`make -s sources -C file/src` \
		`make -s sources -C popt`

CVSTAG = r$(subst .,-,$(VERSION))

pkgsrcdir = $(prefix)/src/$(RPMCANONVENDOR)

install-data-local:
	@$(mkinstalldirs) $(DESTDIR)$(varprefix)/lib/rpm
	@rm -f $(DESTDIR)$(libdir)/rpmrc
	@@LN_S@ rpm/rpmrc $(DESTDIR)$(libdir)/rpmrc
	@rm -f $(DESTDIR)$(libdir)/rpmpopt
	@@LN_S@ rpm/rpmpopt $(DESTDIR)$(libdir)/rpmpopt
	@rm -f $(DESTDIR)$(libdir)/rpm/rpmt
	@@LN_S@ rpmb $(DESTDIR)$(libdir)/rpm/rpmt
	@rm -f $(DESTDIR)$(libdir)/rpm/rpme
	@@LN_S@ rpmi $(DESTDIR)$(libdir)/rpm/rpme
	@rm -f $(DESTDIR)$(libdir)/rpm/rpmu
	@@LN_S@ rpmi $(DESTDIR)$(libdir)/rpm/rpmu
	@rm -f $(DESTDIR)$(libdir)/rpm/rpmv
	@@LN_S@ rpmq $(DESTDIR)$(libdir)/rpm/rpmv
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
	    $(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/$$dir;\
	done
	@case "@host_cpu@" in \
	*86)	$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/i386 ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/i486 ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/i586 ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/i686 ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/athlon ;;\
	alpha*) $(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/alpha ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/alphaev6 ;;\
	arm*)	$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/armv3l ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/armv4l ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/armv4tl ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/armv5tel ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/armv5tejl ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/armv6l ;;\
	sparc*) $(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/sparc ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/sparcv8 ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/sparcv9 ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/sparc64 ;;\
	ia64*)	$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/ia64 ;;\
	s390*)	$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/s390 ;;\
	mipsel*) $(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/mipsel ;;\
	mips*)  $(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/mips ;;\
	powerpc*) $(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/ppc ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/ppciseries ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/ppcpseries ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/ppc64 ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/ppc64iseries ;\
		$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/ppc64pseries ;;\
	*)	$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/@host_cpu@ ;;\
	esac
	$(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/noarch
	@case "@host_os@" in \
	mint) $(mkinstalldirs) $(DESTDIR)$(pkgsrcdir)/RPMS/m68kmint ;;\
	solaris*|linux*|darwin*) \
	   chmod u+x $(top_srcdir)/installplatform; DESTDIR="$(DESTDIR)" pkglibdir="$(pkglibdir)" $(top_srcdir)/installplatform rpmrc macros platform ;; \
	esac
	@$(mkinstalldirs) $(DESTDIR)/var/tmp

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
	mkdir -p $@
	- [ X"@__DOXYGEN@" != Xno ] && @__DOXYGEN@

ctags:
	find . -type f -name "*.[ch]*" | xargs @CTAGS@

cscope:
	@CSCOPE@ -b -R

cref: ctags cscope
