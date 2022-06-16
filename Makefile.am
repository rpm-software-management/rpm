# Top level Makefile for rpm

ACLOCAL_AMFLAGS = -I m4

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-python \
	--enable-bdb \
	--enable-ndb \
	--enable-zstd \
	--with-cap \
	--with-acl \
	--with-archive \
	--with-audit \
	--with-selinux \
	--with-imaevm \
	--with-fapolicyd \
	--with-fsverity \
	--disable-dependency-tracking

include $(top_srcdir)/rpm.am
AM_CFLAGS = @RPMCFLAGS@

CLEANFILES =
EXTRA_DIST = ChangeLog CREDITS INSTALL \
	autogen.sh installplatform platform.in build-aux
BUILT_SOURCES =

SUBDIRS = po misc
SUBDIRS += rpmio lib sign build scripts fileattrs docs .

if ENABLE_PYTHON
SUBDIRS += python
endif

if ENABLE_PLUGINS
SUBDIRS += plugins
endif

if HAVE_FAKECHROOT
SUBDIRS += tests
endif

DIST_SUBDIRS = po misc rpmio lib sign build python scripts fileattrs docs tests plugins

pkgconfigdir = $(libdir)/pkgconfig

AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir) -I$(top_srcdir)/include/
AM_CPPFLAGS += -I$(top_srcdir)/build
AM_CPPFLAGS += -I$(top_srcdir)/lib
AM_CPPFLAGS += -I$(top_srcdir)/rpmio
AM_CPPFLAGS += @WITH_OPENSSL_INCLUDE@
AM_CPPFLAGS += @WITH_POPT_INCLUDE@
AM_CPPFLAGS += -I$(top_srcdir)/misc
AM_CPPFLAGS += -DLOCALEDIR="\"$(localedir)\""
AM_CPPFLAGS += -DLIBRPMALIAS_FILENAME="\"rpmpopt-${VERSION}\""

pkginclude_HEADERS =

pkginclude_HEADERS += include/rpm/argv.h
pkginclude_HEADERS += include/rpm/rpmcrypto.h
pkginclude_HEADERS += include/rpm/rpmio.h
pkginclude_HEADERS += include/rpm/rpmurl.h
pkginclude_HEADERS += include/rpm/rpmmacro.h
pkginclude_HEADERS += include/rpm/rpmlog.h
pkginclude_HEADERS += include/rpm/rpmpgp.h
pkginclude_HEADERS += include/rpm/rpmsq.h
pkginclude_HEADERS += include/rpm/rpmstring.h
pkginclude_HEADERS += include/rpm/rpmstrpool.h
pkginclude_HEADERS += include/rpm/rpmsw.h
pkginclude_HEADERS += include/rpm/rpmfileutil.h
pkginclude_HEADERS += include/rpm/rpmutil.h
pkginclude_HEADERS += include/rpm/rpmkeyring.h
pkginclude_HEADERS += include/rpm/rpmbase64.h
pkginclude_HEADERS += include/rpm/rpmver.h

pkginclude_HEADERS += include/rpm/header.h
pkginclude_HEADERS += include/rpm/rpmdb.h
pkginclude_HEADERS += include/rpm/rpmcallback.h
pkginclude_HEADERS += include/rpm/rpmcli.h
pkginclude_HEADERS += include/rpm/rpmlib.h
pkginclude_HEADERS += include/rpm/rpmds.h
pkginclude_HEADERS += include/rpm/rpmfi.h
pkginclude_HEADERS += include/rpm/rpmfiles.h
pkginclude_HEADERS += include/rpm/rpmpol.h
pkginclude_HEADERS += include/rpm/rpmps.h
pkginclude_HEADERS += include/rpm/rpmprob.h
pkginclude_HEADERS += include/rpm/rpmtag.h
pkginclude_HEADERS += include/rpm/rpmtd.h
pkginclude_HEADERS += include/rpm/rpmte.h
pkginclude_HEADERS += include/rpm/rpmts.h
pkginclude_HEADERS += include/rpm/rpmtypes.h
pkginclude_HEADERS += include/rpm/rpmarchive.h

pkginclude_HEADERS += include/rpm/rpmsign.h

pkginclude_HEADERS += include/rpm/rpmbuild.h
pkginclude_HEADERS += include/rpm/rpmfc.h
pkginclude_HEADERS += include/rpm/rpmspec.h


bin_PROGRAMS =		rpm rpm2cpio rpmbuild rpmdb rpmkeys rpmsign rpmspec
if WITH_ARCHIVE
bin_PROGRAMS += 	rpm2archive 
endif

rpmlibexec_PROGRAMS =
rpmconfig_SCRIPTS =	mkinstalldirs

noinst_LTLIBRARIES = 		libcliutils.la
libcliutils_la_SOURCES = 	cliutils.h cliutils.c
libcliutils_la_CPPFLAGS =	$(AM_CPPFLAGS)
libcliutils_la_LIBADD = 	lib/librpm.la rpmio/librpmio.la
libcliutils_la_LIBADD +=	@WITH_POPT_LIB@

rpm_SOURCES =		rpm.c debug.h system.h
rpm_CPPFLAGS =		$(AM_CPPFLAGS)
rpm_LDADD =		libcliutils.la
rpm_LDADD +=		lib/librpm.la rpmio/librpmio.la
rpm_LDADD +=		@WITH_POPT_LIB@

rpmdb_SOURCES =		rpmdb.c debug.h system.h
rpmdb_CPPFLAGS =	$(AM_CPPFLAGS)
rpmdb_LDADD =		libcliutils.la
rpmdb_LDADD +=		lib/librpm.la rpmio/librpmio.la
rpmdb_LDADD +=		@WITH_POPT_LIB@

rpmkeys_SOURCES =	rpmkeys.c debug.h system.h
rpmkeys_CPPFLAGS =	$(AM_CPPFLAGS)
rpmkeys_LDADD =		libcliutils.la
rpmkeys_LDADD +=	lib/librpm.la rpmio/librpmio.la
rpmkeys_LDADD +=	@WITH_POPT_LIB@

rpmsign_SOURCES =	rpmsign.c debug.h system.h
rpmsign_CPPFLAGS =	$(AM_CPPFLAGS)
rpmsign_LDADD =		libcliutils.la
rpmsign_LDADD +=	sign/librpmsign.la lib/librpm.la rpmio/librpmio.la
rpmsign_LDADD +=	@WITH_POPT_LIB@

rpmbuild_SOURCES =	rpmbuild.c debug.h system.h
rpmbuild_CPPFLAGS =	$(AM_CPPFLAGS)
rpmbuild_LDADD =	libcliutils.la
rpmbuild_LDADD +=	build/librpmbuild.la lib/librpm.la rpmio/librpmio.la
rpmbuild_LDADD += 	@WITH_POPT_LIB@

rpmspec_SOURCES =	rpmspec.c debug.h system.h
rpmspec_CPPFLAGS =	$(AM_CPPFLAGS)
rpmspec_CPPFLAGS +=	@READLINE_CFLAGS@
rpmspec_LDADD =		libcliutils.la
rpmspec_LDADD +=	build/librpmbuild.la lib/librpm.la rpmio/librpmio.la
rpmspec_LDADD +=	@WITH_POPT_LIB@ @READLINE_LIBS@

rpm2cpio_SOURCES =	rpm2cpio.c debug.h system.h
rpm2cpio_LDADD =	lib/librpm.la rpmio/librpmio.la
rpm2cpio_LDADD +=	@WITH_POPT_LIB@

rpm2archive_SOURCES =	rpm2archive.c debug.h system.h
rpm2archive_LDADD =	lib/librpm.la rpmio/librpmio.la
rpm2archive_LDADD +=	@WITH_POPT_LIB@ @WITH_ARCHIVE_LIB@

bin_PROGRAMS +=		rpmlua
rpmlua_CPPFLAGS =	$(AM_CPPFLAGS)
rpmlua_CPPFLAGS +=	@READLINE_CFLAGS@
rpmlua_SOURCES =	tools/rpmlua.c debug.h system.h
rpmlua_LDADD =		lib/librpm.la rpmio/librpmio.la
rpmlua_LDADD +=		@WITH_POPT_LIB@
rpmlua_LDADD += 	@READLINE_LIBS@

if LIBELF
rpmlibexec_PROGRAMS +=	elfdeps
elfdeps_SOURCES =	tools/elfdeps.c
elfdeps_LDADD =		rpmio/librpmio.la
elfdeps_LDADD +=	@WITH_LIBELF_LIB@ @WITH_POPT_LIB@
endif #LIBELF

rpmlibexec_PROGRAMS +=	rpmdeps
rpmdeps_SOURCES =	tools/rpmdeps.c
rpmdeps_LDADD =		lib/librpm.la rpmio/librpmio.la build/librpmbuild.la @WITH_POPT_LIB@

rpmlibexec_PROGRAMS +=	rpmuncompress
rpmuncompress_SOURCES =	tools/rpmuncompress.c
rpmuncompress_LDADD =	lib/librpm.la rpmio/librpmio.la @WITH_POPT_LIB@

bin_PROGRAMS +=		rpmgraph
rpmgraph_SOURCES =	tools/rpmgraph.c
rpmgraph_LDADD =	lib/librpm.la rpmio/librpmio.la @WITH_POPT_LIB@

dist_bin_SCRIPTS =	scripts/gendiff

rpmconfig_DATA = rpmrc
rpmrc: $(top_srcdir)/rpmrc.in
	@$(SED) \
	  -e "s,[@]SYSCONFIGDIR[@],$(sysconfdir)/rpm,g" \
	  < $(top_srcdir)/rpmrc.in > rpmrc.tmp \
	  && ( cd $(top_builddir) && ./config.status --file=${subdir}/rpmrc:${subdir}/rpmrc.tmp )
CLEANFILES += rpmrc.tmp rpmrc
EXTRA_DIST += rpmrc.in

noinst_DATA = macros
macros: $(top_srcdir)/macros.in
	  ( cd $(top_builddir) && ./config.status --file=${subdir}/macros:${subdir}/macros.in )
CLEANFILES += macros
EXTRA_DIST += macros.in

noinst_DATA += platform
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
	  -e "s,[@]BINDIR[@],$(bindir),g" \
	  < $(top_srcdir)/rpmpopt.in > rpmpopt-@VERSION@.tmp \
	  && ( cd $(top_builddir) && ./config.status --file=${subdir}/rpmpopt-@VERSION@:${subdir}/rpmpopt-@VERSION@.tmp )
CLEANFILES += rpmpopt-@VERSION@.tmp rpmpopt-@VERSION@
EXTRA_DIST += rpmpopt.in

usrsrcdir = $(prefix)/src

rpmvardir = $(localstatedir)/lib/rpm
rpmvar_DATA =

install-exec-hook:
	@rm -f $(DESTDIR)$(bindir)/rpmquery
	@LN_S@ $(rpm_symlink_prefix)rpm $(DESTDIR)$(bindir)/rpmquery
	@rm -f $(DESTDIR)$(bindir)/rpmverify
	@LN_S@  $(rpm_symlink_prefix)rpm $(DESTDIR)$(bindir)/rpmverify

install-data-local:
	DESTDIR="$(DESTDIR)" pkglibdir="$(rpmconfigdir)" \
		$(SHELL) $(top_srcdir)/installplatform \
			rpmrc platform macros \
			$(RPMCANONVENDOR) $(RPMCANONOS) $(RPMCANONGNU)
	@$(MKDIR_P) $(DESTDIR)$(localstatedir)/tmp
	@$(MKDIR_P) $(DESTDIR)$(rpmconfigdir)/macros.d
	@$(MKDIR_P) $(DESTDIR)$(rpmconfigdir)/lua

# XXX to appease distcheck we need to remove "stuff" here...
uninstall-local:
	@rm -f $(DESTDIR)$(bindir)/rpmquery
	@rm -f $(DESTDIR)$(bindir)/rpmverify
	@rm -rf $(DESTDIR)$(rpmconfigdir)/platform/
	@rm -f $(DESTDIR)$(rpmconfigdir)/macros
	@rm -rf $(DESTDIR)$(rpmconfigdir)/macros.d
	@rm -rf $(DESTDIR)$(rpmconfigdir)/lua

MAINTAINERCLEANFILES = ChangeLog

# Always build librpm docs
if DOXYGEN
docs/librpm.doxy: docs/librpm.doxy.in docs/librpm/Doxyheader.h Makefile.am configure.ac $(pkginclude_HEADERS)
	files=`echo " $(pkginclude_HEADERS)" | $(SED) -e "s, , \@top_srcdir\@/,g"`; \
	$(SED) -e "s,[@]pkginclude_HEADERS[@],$$files," \
	  -e "s,\@top_srcdir\@/,$(top_srcdir)/,g" \
	  -e "s,[@]HAVE_DOT[@],$(HAVE_DOT)," \
	  -e "s,[@]PACKAGE[@],$(PACKAGE)," \
	  -e "s,[@]VERSION[@],$(VERSION)," \
	< $(top_srcdir)/docs/librpm.doxy.in > docs/librpm.doxy

docs/librpm/html: docs/librpm.doxy
	@DOXYGEN@ docs/librpm.doxy
noinst_DATA += docs/librpm/html
endif
EXTRA_DIST += docs/librpm.doxy.in docs/librpm/Doxyheader.h
EXTRA_DIST += docs/librpm/html

.PHONY: ci
ci:
	podman build -t rpm -f $(top_srcdir)/ci/Dockerfile $(top_srcdir)
	podman run -t rpm

ChangeLog:
	touch ChangeLog

dist-hook:
	if [ -d .git ] && [ -x $(command -v git) ]; then git log --no-merges > $(distdir)/ChangeLog; fi

distclean-local:
	rm -rf docs/librpm.doxy docs/librpm/html
	rm -rf Doxytags
	rm -f stamp-h.in
