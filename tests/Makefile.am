## Process this file with automake to produce Makefile.in.

EXTRA_DIST =
CLEANFILES =

# HACK: This should not be here.
rpmbindir = `echo $(bindir) | sed -e s,usr/bin,bin,`

##
TESTSUITE = $(srcdir)/rpmtests
EXTRA_DIST += local.at $(TESTSUITE)

## testsuite components
TESTSUITE_AT = rpmtests.at
TESTSUITE_AT += rpmgeneral.at
TESTSUITE_AT += rpmquery.at
TESTSUITE_AT += rpmdb.at
TESTSUITE_AT += rpmbuild.at
TESTSUITE_AT += rpmi.at
EXTRA_DIST += $(TESTSUITE_AT)

## testsuite data
EXTRA_DIST += data/SPECS/hello.spec
EXTRA_DIST += data/SPECS/foo.spec
EXTRA_DIST += data/SOURCES/hello-1.0.tar.gz
EXTRA_DIST += data/RPMS/foo-1.0-1.noarch.rpm
EXTRA_DIST += data/RPMS/hello-1.0-1.i386.rpm
EXTRA_DIST += data/RPMS/hello-1.0-1.ppc64.rpm
EXTRA_DIST += data/SRPMS/foo-1.0-1.src.rpm
EXTRA_DIST += data/SRPMS/hello-1.0-1.src.rpm

# testsuite voodoo
AUTOTEST = $(AUTOM4TE) --language=autotest
$(TESTSUITE): $(srcdir)/package.m4 local.at $(TESTSUITE_AT)
	$(AUTOTEST) -I '$(srcdir)' -o $@.tmp $@.at
	mv $@.tmp $@

$(srcdir)/package.m4: $(top_srcdir)/configure.ac
	@{ \
	echo '# Signature of the current package.'; \
	echo 'm4_define([AT_PACKAGE_NAME],      [@PACKAGE_NAME@])'; \
	echo 'm4_define([AT_PACKAGE_TARNAME],   [@PACKAGE_TARNAME@])'; \
	echo 'm4_define([AT_PACKAGE_VERSION],   [@PACKAGE_VERSION@])'; \
	echo 'm4_define([AT_PACKAGE_STRING],    [@PACKAGE_STRING@])'; \
	echo 'm4_define([AT_PACKAGE_BUGREPORT], [@PACKAGE_BUGREPORT@])'; \
	} >'$(srcdir)/package.m4'
EXTRA_DIST += package.m4

atconfig: ${top_builddir}/config.status
	(cd ${top_builddir} && ./config.status ${subdir}/atconfig)
CLEANFILES += atconfig


atlocal:	atlocal.in
	@sed \
	  -e "s,[@]rpmbindir[@],$(rpmbindir)," \
	  -e "s,[@]usrbindir[@],$(bindir)," \
	  -e "s,[@]usrlibdir[@],$(libdir)," \
	  -e "s,[@]RPMCONFIGDIR[@],$(RPMCONFIGDIR)," \
	< $(srcdir)/atlocal.in > atlocal
DISTCLEANFILES = atlocal
EXTRA_DIST += atlocal.in

## Create a customized rpmrc
rpmrc:	$(top_srcdir)/rpmrc.in
	@sed \
	  -e "s,[@]RPMCONFIGDIR[@],`pwd`/testing$(RPMCONFIGDIR),g" \
	  -e "s,[@]SYSCONFIGDIR[@],`pwd`/testing$(sysconfdir)/rpm,g" \
	  -e "s,:~/.rpmmacros,," \
	< $(top_srcdir)/rpmrc.in > rpmrc.tmp \
	&& ( cd $(top_builddir) && ./config.status --file=${subdir}/rpmrc:${subdir}/rpmrc.tmp )
CLEANFILES += rpmrc rpmrc.tmp

## Create a customized macros
macros.in:	$(top_srcdir)/macros.in
	@sed \
	  -e "s,[@]RPMCONFIGDIR[@],`pwd`/testing$(RPMCONFIGDIR),g" \
	< $(top_srcdir)/macros.in > macros.in
CLEANFILES += macros.in


## Create a customized rpmpopt
rpmpopt.in:	$(top_srcdir)/rpmpopt.in
	@sed \
	  -e "s,[@]RPMCONFIGDIR[@],`pwd`/testing$(RPMCONFIGDIR),g" \
	< $(top_srcdir)/rpmpopt.in > rpmpopt.in
CLEANFILES += rpmpopt.in


## Create a test-installation directory "testing"
testing@RPMCONFIGDIR@/rpmrc: rpmrc macros.in rpmpopt.in $(top_builddir)/config.status
	(cd ${top_builddir} && \
	  $(MAKE) DESTDIR=`pwd`/${subdir}/testing install)
	$(INSTALL_DATA) rpmrc testing$(RPMCONFIGDIR)/rpmrc
	(cd ${top_builddir} && \
	  ./config.status --file=${subdir}/testing$(RPMCONFIGDIR)/macros:${subdir}/macros.in)
	(cd ${top_builddir} && \
	  ./config.status --file=${subdir}/testing$(RPMCONFIGDIR)/rpmpopt-@VERSION@:${subdir}/rpmpopt.in)

check_DATA = testing@RPMCONFIGDIR@/rpmrc atconfig atlocal $(TESTSUITE)

check-local: $(check_DATA)
	$(SHELL) '$(TESTSUITE)' $(TESTSUITEFLAGS)

installcheck-local: $(check_DATA)
	$(SHELL) '$(TESTSUITE)' AUTOTEST_PATH='$(bindir)' \
	$(TESTSUITEFLAGS)

clean-local:
	test ! -f '$(TESTSUITE)' || $(SHELL) '$(TESTSUITE)' --clean
	rm -f *.tmp
	rm -rf testing
