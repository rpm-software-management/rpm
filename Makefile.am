# Top level Makefile for rpm

include $(top_srcdir)/rpm.am

CLEANFILES =
EXTRA_DIST = CHANGES ChangeLog CREDITS Doxyheader GROUPS INSTALL \
	autodeps autogen.sh \
	db db3/configure installplatform platform* 

SUBDIRS = po misc @WITH_DB_SUBDIR@ lua rpmio rpmdb lib build python tools scripts doc . tests

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
	@WITH_LIBELF_INCLUDE@
AM_CPPFLAGS += -DLOCALEDIR="\"$(localedir)\""
AM_CPPFLAGS += -DRPMCONFIGDIR="\"@RPMCONFIGDIR@\""
AM_CPPFLAGS += -DLIBRPMALIAS_FILENAME="\"@RPMCONFIGDIR@/rpmpopt-${VERSION}\""

staticLDFLAGS = @LDFLAGS_STATIC@ @LDFLAGS_NPTL@

rpmbindir = `echo $(bindir) | sed -e s,usr/bin,bin,`
rpmbin_PROGRAMS = rpm

bin_PROGRAMS =		rpm2cpio

rpmlibexec_PROGRAMS =	rpmb rpmd rpmi rpmk rpmq
rpmconfig_DATA =	rpmrc rpmpopt-$(VERSION) macros
rpmlibexec_SCRIPTS =	find-provides find-requires mkinstalldirs \
			config.guess config.sub

rpm_SOURCES =		rpmqv.c debug.h system.h
rpm_CPPFLAGS =		$(AM_CPPFLAGS) -DIAM_RPMDB -DIAM_RPMEIU -DIAM_RPMK -DIAM_RPMQV
rpm_LDFLAGS =		@WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@
rpm_LDADD =		build/librpmbuild.la lib/librpm.la rpmdb/librpmdb.la rpmio/librpmio.la @WITH_POPT_LIB@ @WITH_ZLIB_LIB@

rpmb_SOURCES =		build.c rpmqv.c build.h debug.h system.h
rpmb_CPPFLAGS =		$(AM_CPPFLAGS) -DIAM_RPMBT
rpmb_LDFLAGS =		@WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@
rpmb_LDADD =		build/librpmbuild.la lib/librpm.la rpmdb/librpmdb.la rpmio/librpmio.la @WITH_POPT_LIB@ @WITH_ZLIB_LIB@

rpmd_SOURCES =		rpmqv.c debug.h system.h
rpmd_CPPFLAGS =		$(AM_CPPFLAGS) -DIAM_RPMDB
rpmd_LDFLAGS =		@WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@
rpmd_LDADD =		lib/librpm.la rpmdb/librpmdb.la rpmio/librpmio.la @WITH_POPT_LIB@ @WITH_ZLIB_LIB@

rpmi_SOURCES =		rpmqv.c debug.h system.h
rpmi_CPPFLAGS =		$(AM_CPPFLAGS) -DIAM_RPMEIU
#rpmi_LDFLAGS =		@WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@ $(staticLDFLAGS)
rpmi_LDFLAGS =		@WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@
rpmi_LDADD =		lib/librpm.la rpmdb/librpmdb.la rpmio/librpmio.la @WITH_POPT_LIB@ @WITH_ZLIB_LIB@

rpmk_SOURCES =		rpmqv.c debug.h system.h
rpmk_CPPFLAGS =		$(AM_CPPFLAGS) -DIAM_RPMK
rpmk_LDFLAGS =		@WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@
rpmk_LDADD =		lib/librpm.la rpmdb/librpmdb.la rpmio/librpmio.la @WITH_POPT_LIB@ @WITH_ZLIB_LIB@

rpmq_SOURCES =		rpmqv.c debug.h system.h
rpmq_CPPFLAGS =		$(AM_CPPFLAGS) -DIAM_RPMQV
rpmq_LDFLAGS =		@WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@
rpmq_LDADD =		build/librpmbuild.la lib/librpm.la rpmdb/librpmdb.la rpmio/librpmio.la @WITH_POPT_LIB@ @WITH_ZLIB_LIB@

rpm2cpio_SOURCES =	rpm2cpio.c debug.h system.h
rpm2cpio_LDFLAGS =	@WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@
rpm2cpio_LDADD =	lib/librpm.la rpmdb/librpmdb.la rpmio/librpmio.la @WITH_POPT_LIB@ @WITH_ZLIB_LIB@

rpmrc: $(top_srcdir)/rpmrc.in
	@sed \
	  -e "s,[@]SYSCONFIGDIR[@],$(sysconfdir)/rpm,g" \
	  < $(top_srcdir)/rpmrc.in > rpmrc.tmp \
	  && ( cd $(top_builddir) && ./config.status --file=${subdir}/rpmrc:${subdir}/rpmrc.tmp )
CLEANFILES += rpmrc.tmp rpmrc
EXTRA_DIST += rpmrc.in

macros: $(top_srcdir)/macros.in
	@sed \
	  -e "s,[@]varprefix[@],$(localstatedir),g" \
	  < $(top_srcdir)/macros.in > macros.tmp \
	  && ( cd $(top_builddir) && ./config.status --file=${subdir}/macros:${subdir}/macros.tmp )
CLEANFILES += macros.tmp macros
EXTRA_DIST += macros.in

pkgsrcdir = $(prefix)/src/$(RPMCANONVENDOR)

rpmvardir = $(localstatedir)/lib/rpm
rpmvar_DATA =

install-data-local:
	@rm -f $(DESTDIR)$(rpmlibexecdir)/rpmt
	@@LN_S@ rpmb $(DESTDIR)$(rpmlibexecdir)/rpmt
	@rm -f $(DESTDIR)$(rpmlibexecdir)/rpme
	@@LN_S@ rpmi $(DESTDIR)$(rpmlibexecdir)/rpme
	@rm -f $(DESTDIR)$(rpmlibexecdir)/rpmu
	@@LN_S@ rpmi $(DESTDIR)$(rpmlibexecdir)/rpmu
	@rm -f $(DESTDIR)$(rpmlibexecdir)/rpmv
	@@LN_S@ rpmq $(DESTDIR)$(rpmlibexecdir)/rpmv
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
	   DESTDIR="$(DESTDIR)" pkglibdir="$(rpmconfigdir)" $(SHELL) $(top_srcdir)/installplatform rpmrc macros platform ;; \
	esac
	@$(MKDIR_P) $(DESTDIR)$(localstatedir)/tmp

# XXX to appease distcheck we need to remove "stuff" here...
uninstall-local:
	@rm -rf $(DESTDIR)/$(pkglibdir)/*-*
	
distclean-local:
	@rm -rf Doxytags apidocs 
	@rm -f stamp-h.in db3/*.orig db3/db3lobjs
	@rm -f ChangeLog

.PHONY:	setperms
setperms:
	@for f in $(rpmbin_PROGRAMS) ; do\
	    $(__CHOWN) ${RPMUSER}.${RPMGROUP} $(DESTDIR)$(rpmbindir)/$$f ;\
	    $(__CHMOD) g+s $(DESTDIR)$(rpmbindir)/$$f ;\
	done
	@for f in $(bin_PROGRAMS) ; do\
	    $(__CHOWN) ${RPMUSER}.${RPMGROUP} $(DESTDIR)$(bindir)/$$f ;\
	done
	@for f in $(rpmlibexec_PROGRAMS) ; do\
	    $(__CHOWN) ${RPMUSER}.${RPMGROUP} $(DESTDIR)$(rpmlibexecdir)/$$f ;\
	    $(__CHMOD) g+s $(DESTDIR)$(rpmlibexecdir)/$$f ;\
	done
	@for f in $(pkglib_SCRIPTS) ; do\
	    $(__CHOWN) ${RPMUSER}.${RPMGROUP} $(DESTDIR)$(pkglibdir)/$$f ;\
	done
	@$(__CHOWN) ${RPMUSER}.${RPMGROUP} $(DESTDIR)$(pkglibdir)
	@$(__CHOWN) -R ${RPMUSER}.${RPMGROUP} $(DESTDIR)$(rpmvardir)
	-@$(__CHMOD) 0664 $(DESTDIR)$(rpmvardir)/[A-Z]*
	-@$(__CHMOD) 0775 $(DESTDIR)$(rpmvardir)
	-@$(__CHMOD) 0664 $(DESTDIR)$(rpmvardir)/__db.*

.PHONY:	unsetgid
unsetgid:
	@for f in $(rpmbin_PROGRAMS) ; do\
	    $(__CHMOD) g-s $(DESTDIR)$(rpmbindir)/$$f ;\
	done
	@for f in $(rpmlibexec_PROGRAMS) ; do\
	    $(__CHMOD) g-s $(DESTDIR)$(rpmlibexecdir)/$$f ;\
	done

.PHONY:	doxygen
doxygen @WITH_APIDOCS_TARGET@: Doxyfile rpmpopt-@VERSION@
	rm -rf $@
	$(MKDIR_P) $@
	- [ X"@__DOXYGEN@" != Xno ] && @__DOXYGEN@

.PHONY: ChangeLog
ChangeLog:
	hg log --no-merge -v > ChangeLog

ACLOCAL_AMFLAGS = -I m4
