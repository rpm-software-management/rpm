## Process this file with automake to produce Makefile.in.

include $(top_srcdir)/rpm.am

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


atlocal:	atlocal.in Makefile
	@sed \
	  -e "s,[@]rpmbindir[@],$(rpmbindir)," \
	  -e "s,[@]usrbindir[@],$(bindir)," \
	  -e "s,[@]usrlibdir[@],$(libdir)," \
	  -e "s,[@]RPMCONFIGDIR[@],$(rpmconfigdir)," \
	< $(srcdir)/atlocal.in > atlocal
DISTCLEANFILES = atlocal
EXTRA_DIST += atlocal.in

## Create a customized rpmrc
rpmrc:	$(top_srcdir)/rpmrc.in
	@sed \
	  -e "s,[@]RPMCONFIGDIR[@],`pwd`/testing$(rpmconfigdir),g" \
	  -e "s,[@]SYSCONFIGDIR[@],`pwd`/testing$(sysconfdir)/rpm,g" \
	  -e "s,:~/.rpmmacros,," \
	< $(top_srcdir)/rpmrc.in > rpmrc.tmp \
	&& ( cd $(top_builddir) && ./config.status --file=${subdir}/rpmrc:${subdir}/rpmrc.tmp )
CLEANFILES += rpmrc rpmrc.tmp

## Create a customized macros
macros.in:	$(top_srcdir)/macros.in Makefile
	@sed \
	  -e "s,[@]RPMCONFIGDIR[@],`pwd`/testing$(rpmconfigdir),g" \
	< $(top_srcdir)/macros.in > macros.in
CLEANFILES += macros.in


## Create a customized rpmpopt
rpmpopt.in:	$(top_srcdir)/rpmpopt.in Makefile
	@sed \
	  -e "s,[@]RPMCONFIGDIR[@],`pwd`/testing$(rpmconfigdir),g" \
	< $(top_srcdir)/rpmpopt.in > rpmpopt.in
CLEANFILES += rpmpopt.in


# Hack: Abusing testing$(bindir)/rpmbuild as stamp file
testing$(bindir)/rpmbuild: ../rpmbuild
	rm -rf testing
	(cd ${top_builddir} && \
	          $(MAKE) DESTDIR=`pwd`/${subdir}/testing install)

testing$(rpmconfigdir)/rpmrc: rpmrc testing$(bindir)/rpmbuild
	$(INSTALL_DATA) rpmrc testing$(rpmconfigdir)/rpmrc

testing$(rpmconfigdir)/macros: macros.in testing$(bindir)/rpmbuild
	(cd ${top_builddir} && \
	  ./config.status --file=${subdir}/testing$(rpmconfigdir)/macros:${subdir}/macros.in)

testing$(rpmconfigdir)/rpmpopt-@VERSION@: rpmpopt.in testing$(bindir)/rpmbuild
	(cd ${top_builddir} && \
	  ./config.status --file=${subdir}/testing$(rpmconfigdir)/rpmpopt-@VERSION@:${subdir}/rpmpopt.in)

check_DATA = atconfig atlocal $(TESTSUITE)
check_DATA += testing$(bindir)/rpmbuild
check_DATA += testing$(rpmconfigdir)/rpmrc
check_DATA += testing$(rpmconfigdir)/macros
check_DATA += testing$(rpmconfigdir)/rpmpopt-@VERSION@

check-local: $(check_DATA)
	$(SHELL) '$(TESTSUITE)' $(TESTSUITEFLAGS) ||:

installcheck-local: $(check_DATA)
	$(SHELL) '$(TESTSUITE)' AUTOTEST_PATH='$(bindir)' \
	$(TESTSUITEFLAGS) ||:

clean-local:
	test ! -f '$(TESTSUITE)' || $(SHELL) '$(TESTSUITE)' --clean
	rm -f *.tmp
	rm -rf testing
