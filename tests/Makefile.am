## Process this file with automake to produce Makefile.in.

EXTRA_DIST =
CLEANFILES =

TESTSUITE = $(srcdir)/rpmtests
EXTRA_DIST += rpmtests.at $(TESTSUITE)

AUTOTEST = $(AUTOM4TE) --language=autotest
$(TESTSUITE): $(srcdir)/rpmtests.at $(TESTSUITE_AT)
	$(AUTOTEST) -I '$(srcdir)' -o $@.tmp $@.at
	mv $@.tmp $@

## testsuite components
TESTSUITE_AT = rpmgeneral.at
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
	@sed -e "s,[@]rpmbindir[@],$(rpmbindir)," \
	  -e "s,[@]usrbindir[@],$(bindir)," \
	  -e "s,[@]usrlibdir[@],$(libdir)," \
	< $(srcdir)/defs.in > defs
DISTCLEANFILES = defs
EXTRA_DIST += defs.in

## Create a customize rpmrc
rpmrc.in:	$(top_srcdir)/rpmrc.in
	@sed \
	  -e "s,[@]RPMCONFIGDIR[@],`pwd`/testing$(RPMCONFIGDIR),g" \
	  -e "s,[@]SYSCONFIGDIR[@],`pwd`/testing$(SYSCONFIGDIR),g" \
	  -e "s,:~/.rpmmacros,," \
	< $(top_srcdir)/rpmrc.in > rpmrc.in
CLEANFILES += rpmrc.in

rpmrc: rpmrc.in
	(cd ${top_builddir} && ./config.status --file=${subdir}/rpmrc:${subdir}/rpmrc.in)
CLEANFILES += rpmrc
