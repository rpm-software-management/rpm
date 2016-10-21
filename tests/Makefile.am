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
TESTSUITE_AT += rpmverify.at
TESTSUITE_AT += rpmdb.at
TESTSUITE_AT += rpmbuild.at
TESTSUITE_AT += rpmi.at
TESTSUITE_AT += rpmvercmp.at
TESTSUITE_AT += rpmdeps.at
TESTSUITE_AT += rpmconflict.at
TESTSUITE_AT += rpmconfig.at
TESTSUITE_AT += rpmconfig2.at
TESTSUITE_AT += rpmconfig3.at
TESTSUITE_AT += rpmreplace.at
TESTSUITE_AT += rpmmacro.at
TESTSUITE_AT += rpmpython.at
TESTSUITE_AT += rpmdepmatch.at
TESTSUITE_AT += rpmscript.at
EXTRA_DIST += $(TESTSUITE_AT)

## testsuite data
EXTRA_DIST += data/SPECS/attrtest.spec
EXTRA_DIST += data/SPECS/hello.spec
EXTRA_DIST += data/SPECS/hello-script.spec
EXTRA_DIST += data/SPECS/hello2.spec
EXTRA_DIST += data/SPECS/hello2-suid.spec
EXTRA_DIST += data/SPECS/foo.spec
EXTRA_DIST += data/SPECS/globtest.spec
EXTRA_DIST += data/SPECS/versiontest.spec
EXTRA_DIST += data/SPECS/conflicttest.spec
EXTRA_DIST += data/SPECS/configtest.spec
EXTRA_DIST += data/SPECS/flangtest.spec
EXTRA_DIST += data/SPECS/hlinktest.spec
EXTRA_DIST += data/SPECS/symlinktest.spec
EXTRA_DIST += data/SPECS/deptest.spec
EXTRA_DIST += data/SPECS/verifyscript.spec
EXTRA_DIST += data/SPECS/fakeshell.spec
EXTRA_DIST += data/SPECS/scripts.spec
EXTRA_DIST += data/SPECS/selfconflict.spec
EXTRA_DIST += data/SPECS/replacetest.spec
EXTRA_DIST += data/SPECS/triggers.spec
EXTRA_DIST += data/SPECS/filetriggers.spec
EXTRA_DIST += data/SPECS/prefixtest.spec
EXTRA_DIST += data/SPECS/testdoc.spec
EXTRA_DIST += data/SOURCES/hello-1.0-modernize.patch
EXTRA_DIST += data/SOURCES/hello-1.0.tar.gz
EXTRA_DIST += data/SOURCES/hello-2.0.tar.gz
EXTRA_DIST += data/RPMS/foo-1.0-1.noarch.rpm
EXTRA_DIST += data/RPMS/hello-1.0-1.i386.rpm
EXTRA_DIST += data/RPMS/hello-1.0-1.ppc64.rpm
EXTRA_DIST += data/RPMS/hello-2.0-1.i686.rpm
EXTRA_DIST += data/RPMS/hello-2.0-1.x86_64.rpm
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

# Hack: Abusing testing$(bindir)/rpmbuild as stamp file
testing$(bindir)/rpmbuild: ../rpmbuild
	rm -rf testing
	mkdir -p testing/$(bindir)
	ln -s ./$(bindir) testing/bin
	(cd ${top_builddir} && \
	          $(MAKE) DESTDIR=`pwd`/${subdir}/testing install)
	cp -r ${srcdir}/data/ testing/
	for d in dev etc magic tmp var; do if [ ! -d testing/$${d} ]; then mkdir testing/$${d}; fi; done
	for node in stdin stderr stdout null; do ln -s /dev/$${node} testing/dev/$${node}; done
	for cf in hosts resolv.conf passwd shadow group gshadow mtab fstab; do [ -f /etc/$${cf} ] && ln -s /etc/$${cf} testing/etc/$${cf}; done
	ln -s ../$(bindir) testing/usr/bin
	for prog in gzip cat patch tar sh ln chmod rm mkdir uname grep sed find file ionice mktemp nice cut sort diff touch; do p=`which $${prog}`; ln -s $${p} testing/$${p}; done
	for d in /proc /sys /selinux /etc/selinux; do if [ -d $${d} ]; then ln -s $${d} testing/$${d}; fi; done
	(cd testing/magic && file -C)

check_DATA = atconfig atlocal $(TESTSUITE)
check_DATA += testing$(bindir)/rpmbuild

if HAVE_FAKECHROOT
check-local: $(check_DATA)
	$(SHELL) '$(TESTSUITE)' $(TESTSUITEFLAGS) ||:
else
check-local:
	echo "you need to have fakechroot installed"
	exit 1
endif !HAVE_FAKECHROOT

installcheck-local: $(check_DATA)
	$(SHELL) '$(TESTSUITE)' AUTOTEST_PATH='$(bindir)' \
	$(TESTSUITEFLAGS) ||:

clean-local:
	test ! -f '$(TESTSUITE)' || $(SHELL) '$(TESTSUITE)' --clean
	rm -f *.tmp
	rm -rf testing
