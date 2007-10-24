# Top level Makefile for rpm

ACLOCAL_AMFLAGS = -I m4

include $(top_srcdir)/rpm.am

CLEANFILES =
EXTRA_DIST = CHANGES ChangeLog CREDITS GROUPS INSTALL \
	autodeps autogen.sh \
	db db3/configure installplatform platform* 

SUBDIRS = po misc
if WITH_INTERNAL_DB
SUBDIRS += db3
endif
SUBDIRS += lua rpmio rpmdb lib build python scripts doc . tests

DIST_SUBDIRS = po misc db3 lua rpmio rpmdb lib build python scripts doc tests

pkgconfigdir = $(libdir)/pkgconfig

AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir)
AM_CPPFLAGS += -I$(top_srcdir)/build
AM_CPPFLAGS += -I$(top_srcdir)/lib
AM_CPPFLAGS += -I$(top_srcdir)/rpmdb
AM_CPPFLAGS += -I$(top_srcdir)/rpmio
AM_CPPFLAGS += @WITH_BEECRYPT_INCLUDE@
AM_CPPFLAGS += @WITH_POPT_INCLUDE@
AM_CPPFLAGS += -I$(top_srcdir)/misc
AM_CPPFLAGS += @WITH_LIBELF_INCLUDE@
AM_CPPFLAGS += -DLOCALEDIR="\"$(localedir)\""
AM_CPPFLAGS += -DRPMCONFIGDIR="\"@RPMCONFIGDIR@\""
AM_CPPFLAGS += -DLIBRPMALIAS_FILENAME="\"@RPMCONFIGDIR@/rpmpopt-${VERSION}\""

staticLDFLAGS = @LDFLAGS_STATIC@ @LDFLAGS_NPTL@

pkginclude_HEADERS =
nodist_pkginclude_HEADERS =

pkginclude_HEADERS += rpmio/argv.h
pkginclude_HEADERS += rpmio/rpmio.h
pkginclude_HEADERS += rpmio/rpmurl.h
pkginclude_HEADERS += rpmio/rpmmacro.h
pkginclude_HEADERS += rpmio/rpmlog.h
pkginclude_HEADERS += rpmio/rpmmessages.h
pkginclude_HEADERS += rpmio/rpmpgp.h
pkginclude_HEADERS += rpmio/rpmsq.h
pkginclude_HEADERS += rpmio/rpmsw.h

pkginclude_HEADERS += rpmdb/header.h
pkginclude_HEADERS += rpmdb/rpmdb.h
nodist_pkginclude_HEADERS += rpmdb/rpmints.h

pkginclude_HEADERS += lib/idtx.h
pkginclude_HEADERS += lib/rpmcli.h
pkginclude_HEADERS += lib/rpmlib.h
pkginclude_HEADERS += lib/rpmal.h
pkginclude_HEADERS += lib/rpmds.h
pkginclude_HEADERS += lib/rpmfi.h
pkginclude_HEADERS += lib/rpmgi.h
pkginclude_HEADERS += lib/rpmps.h
pkginclude_HEADERS += lib/rpmte.h
pkginclude_HEADERS += lib/rpmts.h
pkginclude_HEADERS += lib/stringbuf.h

pkginclude_HEADERS += build/rpmbuild.h
pkginclude_HEADERS += build/rpmfc.h
pkginclude_HEADERS += build/rpmspec.h


rpmbindir = `echo $(bindir) | $(SED) -e s,usr/bin,bin,`
rpmbin_PROGRAMS = rpm

bin_PROGRAMS =		rpm2cpio

rpmlibexec_PROGRAMS =	rpmb rpmd rpmi rpmk rpmq
rpmconfig_SCRIPTS =	find-provides find-requires mkinstalldirs \
			config.guess config.sub
DISTCLEANFILES = find-provides
DISTCLEANFILES += find-requires

rpm_SOURCES =		rpmqv.c debug.h system.h
rpm_CPPFLAGS =		$(AM_CPPFLAGS) -DIAM_RPMDB -DIAM_RPMEIU -DIAM_RPMK -DIAM_RPMQV
rpm_LDADD =		build/librpmbuild.la lib/librpm.la rpmdb/librpmdb.la rpmio/librpmio.la
rpm_LDADD +=		@WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@ @WITH_POPT_LIB@ @WITH_ZLIB_LIB@

rpmb_SOURCES =		build.c rpmqv.c build.h debug.h system.h
rpmb_CPPFLAGS =		$(AM_CPPFLAGS) -DIAM_RPMBT
rpmb_LDADD =		build/librpmbuild.la lib/librpm.la rpmdb/librpmdb.la rpmio/librpmio.la
rpmb_LDADD += 		@WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@ @WITH_POPT_LIB@ @WITH_ZLIB_LIB@

rpmd_SOURCES =		rpmqv.c debug.h system.h
rpmd_CPPFLAGS =		$(AM_CPPFLAGS) -DIAM_RPMDB
rpmd_LDADD =		lib/librpm.la rpmdb/librpmdb.la rpmio/librpmio.la
rpmd_LDADD +=		@WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@ @WITH_POPT_LIB@ @WITH_ZLIB_LIB@

rpmi_SOURCES =		rpmqv.c debug.h system.h
rpmi_CPPFLAGS =		$(AM_CPPFLAGS) -DIAM_RPMEIU
rpmi_LDADD =		lib/librpm.la rpmdb/librpmdb.la rpmio/librpmio.la
rpmi_LDADD +=		@WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@ @WITH_POPT_LIB@ @WITH_ZLIB_LIB@

rpmk_SOURCES =		rpmqv.c debug.h system.h
rpmk_CPPFLAGS =		$(AM_CPPFLAGS) -DIAM_RPMK
rpmk_LDADD =		lib/librpm.la rpmdb/librpmdb.la rpmio/librpmio.la
rpmk_LDADD +=		@WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@ @WITH_POPT_LIB@ @WITH_ZLIB_LIB@

rpmq_SOURCES =		rpmqv.c debug.h system.h
rpmq_CPPFLAGS =		$(AM_CPPFLAGS) -DIAM_RPMQV
rpmq_LDADD =		build/librpmbuild.la lib/librpm.la rpmdb/librpmdb.la rpmio/librpmio.la
rpmq_LDADD +=		@WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@ @WITH_POPT_LIB@ @WITH_ZLIB_LIB@

rpm2cpio_SOURCES =	rpm2cpio.c debug.h system.h
rpm2cpio_LDADD =	lib/librpm.la rpmdb/librpmdb.la rpmio/librpmio.la
rpm2cpio_LDADD +=	@WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@ @WITH_POPT_LIB@ @WITH_ZLIB_LIB@


if LIBELF
if LIBDWARF
rpmconfig_SCRIPTS += scripts/find-debuginfo.sh

rpmlibexec_PROGRAMS +=	debugedit
debugedit_SOURCES =	tools/debugedit.c tools/hashtab.c tools/hashtab.h
debugedit_LDADD =	@WITH_LIBELF_LIB@ @WITH_BEECRYPT_LIB@ @WITH_POPT_LIB@
endif
endif

rpmlibexec_PROGRAMS +=	javadeps
javadeps_SOURCES =	tools/javadeps.c
javadeps_LDADD =

rpmlibexec_PROGRAMS +=	rpmcache
rpmcache_SOURCES =	tools/rpmcache.c
rpmcache_LDADD =	lib/librpm.la

rpmlibexec_PROGRAMS +=	rpmdeps
rpmdeps_SOURCES =	tools/rpmdeps.c
rpmdeps_LDADD =		build/librpmbuild.la

bin_PROGRAMS +=		rpmgraph
rpmgraph_SOURCES =	tools/rpmgraph.c
rpmgraph_LDADD =	lib/librpm.la

dist_bin_SCRIPTS =	scripts/gendiff

check_PROGRAMS = 	dumpdb
dumpdb_SOURCES = 	tools/dumpdb.c
dumpdb_LDADD =		lib/librpm.la

check_PROGRAMS +=	convertdb1
convertdb1_SOURCES =	tools/convertdb1.c
convertdb1_LDADD =	lib/librpm.la

check_PROGRAMS +=	rpminject
rpminject_SOURCES =	tools/rpminject.c
rpminject_LDADD =	build/librpmbuild.la

check_PROGRAMS +=	rpmsort
rpmsort_SOURCES =	tools/rpmsort.c
rpmsort_LDADD =		lib/librpm.la

rpmconfig_DATA = rpmrc
rpmrc: $(top_srcdir)/rpmrc.in
	@$(SED) \
	  -e "s,[@]SYSCONFIGDIR[@],$(sysconfdir)/rpm,g" \
	  < $(top_srcdir)/rpmrc.in > rpmrc.tmp \
	  && ( cd $(top_builddir) && ./config.status --file=${subdir}/rpmrc:${subdir}/rpmrc.tmp )
CLEANFILES += rpmrc.tmp rpmrc
EXTRA_DIST += rpmrc.in

rpmconfig_DATA += macros
macros: $(top_srcdir)/macros.in
	@$(SED) \
	  -e "s,[@]varprefix[@],$(localstatedir),g" \
	  < $(top_srcdir)/macros.in > macros.tmp \
	  && ( cd $(top_builddir) && ./config.status --file=${subdir}/macros:${subdir}/macros.tmp )
CLEANFILES += macros.tmp macros
EXTRA_DIST += macros.in

noinst_DATA = platform
platform: $(top_srcdir)/platform.in
	@$(SED) \
	  -e "s,[@]RPMCONFIGDIR[@],$(rpmconfigdir),g" \
	  < $(top_srcdir)/platform.in > platform.tmp \
	  && ( cd $(top_builddir) && ./config.status --file=${subdir}/platform:${subdir}/platform.tmp )
CLEANFILES += platform.tmp platform
EXTRA_DIST += platform.in

pkgconfig_DATA = rpm.pc
rpm.pc: $(top_srcdir)/rpm.pc.in
	@$(SED) \
	  -e "s,[@]RPMCONFIGDIR[@],$(rpmconfigdir),g" \
	  < $(top_srcdir)/rpm.pc.in > rpm.pc.tmp \
	  && ( cd $(top_builddir) && ./config.status --file=${subdir}/rpm.pc:${subdir}/rpm.pc.tmp )
CLEANFILES += rpm.pc.tmp rpm.pc
EXTRA_DIST += rpm.pc.in

rpmconfig_DATA += rpmpopt-$(VERSION)
rpmpopt-@VERSION@: $(top_srcdir)/rpmpopt.in
	@$(SED) \
	  -e "s,[@]RPMCONFIGDIR[@],$(rpmconfigdir),g" \
	  < $(top_srcdir)/rpmpopt.in > rpmpopt-@VERSION@.tmp \
	  && ( cd $(top_builddir) && ./config.status --file=${subdir}/rpmpopt-@VERSION@:${subdir}/rpmpopt-@VERSION@.tmp )
CLEANFILES += rpmpopt-@VERSION@.tmp rpmpopt-@VERSION@
EXTRA_DIST += rpmpopt.in

usrsrcdir = $(prefix)/src

pkgsrcdir = $(usrsrcdir)/packages
pkgsrc_RPMSdir = $(pkgsrcdir)/RPMS
pkgsrc_RPMS_DATA =
pkgsrc_SRPMSdir = $(pkgsrcdir)/SRPMS
pkgsrc_SRPMS_DATA =
pkgsrc_SPECSdir = $(pkgsrcdir)/SPECS
pkgsrc_SPECS_DATA =
pkgsrc_SOURCESdir = $(pkgsrcdir)/SOURCES
pkgsrc_SOURCES_DATA =
pkgsrc_BUILDdir = $(pkgsrcdir)/BUILD
pkgsrc_BUILD_DATA =

rpmvardir = $(localstatedir)/lib/rpm
rpmvar_DATA =

install-exec-hook:
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

install-data-local:
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
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/sparcv9v ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/sparc64 ;\
		$(MKDIR_P) $(DESTDIR)$(pkgsrcdir)/RPMS/sparc64v ;;\
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
	@rm -rf $(DESTDIR)/$(rpmconfigdir)/*-*
	rm -f $(DESTDIR)$(rpmlibexecdir)/rpmu
	rm -f $(DESTDIR)$(rpmlibexecdir)/rpme
	rm -f $(DESTDIR)$(rpmlibexecdir)/rpmt
	rm -f $(DESTDIR)$(rpmlibexecdir)/rpmv
	rm -f $(DESTDIR)$(bindir)/rpmquery
	rm -f $(DESTDIR)$(bindir)/rpmbuild
	rm -f $(DESTDIR)$(bindir)/rpmu
	rm -f $(DESTDIR)$(bindir)/rpmsign
	rm -f $(DESTDIR)$(bindir)/rpme
	rm -f $(DESTDIR)$(bindir)/rpmverify
	rm -f $(DESTDIR)$(bindir)/rpmdb
	rm -f $(DESTDIR)$(bindir)/rpmi

MAINTAINERCLEANFILES = ChangeLog

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

# Build hacking only on request
if APIDOCS
if DOXYGEN
doc/hacking.doxy: doc/hacking.doxy.in doc/hacking/Doxyheader Makefile.am
	$(SED) \
	  -e "s,\@top_srcdir\@/,$(top_srcdir)/,g" \
	  -e "s,[@]HAVE_DOT[@],$(HAVE_DOT)," \
	  -e "s,[@]PACKAGE[@],$(PACKAGE)," \
	  -e "s,[@]VERSION[@],$(VERSION)," \
	< $(top_srcdir)/doc/hacking.doxy.in > doc/hacking.doxy

doc/hacking/html/index.html: doc/hacking.doxy rpmpopt-@VERSION@
	@DOXYGEN@ doc/hacking.doxy
noinst_DATA += doc/hacking/html/index.html
endif
endif
EXTRA_DIST += doc/hacking.doxy.in doc/hacking/Doxyheader

# Always build librpm docs
if DOXYGEN
doc/librpm.doxy: doc/librpm.doxy.in doc/librpm/Doxyheader Makefile.am $(pkginclude_HEADERS)
	files=`echo " $(HEADERS)" | $(SED) -e "s, , \@top_srcdir\@/,g"`; \
	$(SED) -e "s,[@]pkginclude_HEADERS[@],$$files," \
	  -e "s,\@top_srcdir\@/,$(top_srcdir)/,g" \
	  -e "s,[@]HAVE_DOT[@],$(HAVE_DOT)," \
	  -e "s,[@]PACKAGE[@],$(PACKAGE)," \
	  -e "s,[@]VERSION[@],$(VERSION)," \
	< $(top_srcdir)/doc/librpm.doxy.in > doc/librpm.doxy

doc/librpm/html/index.html: doc/librpm.doxy
	@DOXYGEN@ doc/librpm.doxy
noinst_DATA += doc/librpm/html/index.html
endif
EXTRA_DIST += doc/librpm.doxy.in doc/librpm/Doxyheader
EXTRA_DIST += doc/librpm/html

if WITH_INTERNAL_DB
db.h:	$(top_builddir)/db3/db.h
	$(LN_S) $(top_builddir)/db3/db.h $@
BUILT_SOURCES = db.h
endif
DISTCLEANFILES += db.h

clean-local:
	rm -rf doc/hacking/html

distclean-local:
	rm -rf Doxytags
	rm -f stamp-h.in db3/*.orig db3/db3lobjs

maintainer-clean-local:
	rm -rf doc/librpm/html

