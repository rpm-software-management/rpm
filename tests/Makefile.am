## Process this file with automake to produce Makefile.in.

EXTRA_DIST =
CLEANFILES =

# HACK: This should not be here.
rpmbindir = `echo $(bindir) | sed -e s,usr/bin,bin,`

##
TESTSUITE = $(srcdir)/rpmtests
EXTRA_DIST += local.at $(TESTSUITE)

AUTOTEST = $(AUTOM4TE) --language=autotest
$(TESTSUITE): $(srcdir)/package.m4 local.at $(TESTSUITE_AT)
	$(AUTOTEST) -I '$(srcdir)' -o $@.tmp $@.at
	mv $@.tmp $@

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
EXTRA_DIST += data/SOURCES/hello-1.0.tar.gz
EXTRA_DIST += data/RPMS/hello-1.0-1.i386.rpm
EXTRA_DIST += data/SRPMS/hello-1.0-1.src.rpm

# testsuite voodoo
$(srcdir)/package.m4: $(top_srcdir)/configure.ac
	@{ \
	echo '# Signature of the current package.'; \
	echo 'm4_define([AT_PACKAGE_NAME],      [@PACKAGE_NAME@])'; \
	echo 'm4_define([AT_PACKAGE_TARNAME],   [@PACKAGE_TARNAME@])'; \
	echo 'm4_define([AT_PACKAGE_VERSION],   [@PACKAGE_VERSION@])'; \
	echo 'm4_define([AT_PACKAGE_STRING],    [@PACKAGE_STRING@])'; \
	echo 'm4_define([AT_PACKAGE_BUGREPORT], [@PACKAGE_BUGREPORT@])'; \
	} >'$(srcdir)/package.m4'

atconfig: ${top_builddir}/config.status
	(cd ${top_builddir} && ./config.status ${subdir}/atconfig)
CLEANFILES += atconfig

defs:	defs.in
	@sed \
	  -e "s,[@]rpmbindir[@],$(rpmbindir)," \
	  -e "s,[@]usrbindir[@],$(bindir)," \
	  -e "s,[@]usrlibdir[@],$(libdir)," \
	< $(srcdir)/defs.in > defs
DISTCLEANFILES = defs
EXTRA_DIST += defs.in

## Create a customized rpmrc
rpmrc.in:	$(top_srcdir)/rpmrc.in
	@sed \
	  -e "s,[@]RPMCONFIGDIR[@],`pwd`/testing$(RPMCONFIGDIR),g" \
	  -e "s,[@]SYSCONFIGDIR[@],`pwd`/testing$(SYSCONFIGDIR),g" \
	  -e "s,:~/.rpmmacros,," \
	< $(top_srcdir)/rpmrc.in > rpmrc.in
CLEANFILES += rpmrc.in

## Create a customized macros
macros.in:	$(top_srcdir)/macros.in
	@sed \
	  -e "s,[@]RPMCONFIGDIR[@],`pwd`/testing$(RPMCONFIGDIR),g" \
	  -e "s,[@]SYSCONFIGDIR[@],`pwd`/testing$(SYSCONFIGDIR),g" \
	  -e "s,:~/.rpmmacros,," \
	< $(top_srcdir)/macros.in > macros.in
CLEANFILES += macros.in


## Create a test-installation directory "testing"
testing@RPMCONFIGDIR@/rpmrc: rpmrc.in macros.in $(top_builddir)/config.status
	(cd ${top_builddir} && \
	  $(MAKE) DESTDIR=`pwd`/${subdir}/testing install)
	(cd ${top_builddir} && \
	  ./config.status --file=${subdir}/testing$(RPMCONFIGDIR)/rpmrc:${subdir}/rpmrc.in)
	(cd ${top_builddir} && \
	  ./config.status --file=${subdir}/testing$(RPMCONFIGDIR)/macros:${subdir}/macros.in)

check_DATA = defs testing@RPMCONFIGDIR@/rpmrc atconfig atlocal $(TESTSUITE)

check-local: $(check_DATA)
	$(SHELL) '$(TESTSUITE)' $(TESTSUITEFLAGS)

installcheck-local: $(check_DATA)
	$(SHELL) '$(TESTSUITE)' AUTOTEST_PATH='$(bindir)' \
	$(TESTSUITEFLAGS)

clean-local:
	test ! -f '$(TESTSUITE)' || $(SHELL) '$(TESTSUITE)' --clean
	rm -f *.tmp
	rm -rf testing
