## Process this file with automake to produce Makefile.in.

include $(top_srcdir)/rpm.am
AM_CFLAGS = @RPMCFLAGS@

EXTRA_DIST = README.md
CLEANFILES =

##
TESTSUITE = $(srcdir)/rpmtests
EXTRA_DIST += local.at $(TESTSUITE)

AM_CPPFLAGS = -I$(top_srcdir)/include

rpmpgpcheck_LDADD = ../rpmio/librpmio.la

## testsuite components
TESTSUITE_AT = rpmtests.at
TESTSUITE_AT += rpmgeneral.at
TESTSUITE_AT += rpmquery.at
TESTSUITE_AT += rpmverify.at
TESTSUITE_AT += rpmdb.at
TESTSUITE_AT += rpmbuild.at
TESTSUITE_AT += rpmbuildid.at
TESTSUITE_AT += rpmi.at
TESTSUITE_AT += rpme.at
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
TESTSUITE_AT += rpmsigdig.at
TESTSUITE_AT += rpmspec.at
TESTSUITE_AT += rpmio.at
TESTSUITE_AT += rpmorder.at
TESTSUITE_AT += rpmvfylevel.at
TESTSUITE_AT += rpmpgp.at
EXTRA_DIST += $(TESTSUITE_AT)

## testsuite data
EXTRA_DIST += data/SPECS/attrtest.spec
EXTRA_DIST += data/SPECS/bcondtest.spec
EXTRA_DIST += data/SPECS/buildrequires.spec
EXTRA_DIST += data/SPECS/docmiss.spec
EXTRA_DIST += data/SPECS/fs.spec
EXTRA_DIST += data/SPECS/hello.spec
EXTRA_DIST += data/SPECS/hello-auto.spec
EXTRA_DIST += data/SPECS/hello-autopatch.spec
EXTRA_DIST += data/SPECS/hello-r2.spec
EXTRA_DIST += data/SPECS/hello-script.spec
EXTRA_DIST += data/SPECS/hello2.spec
EXTRA_DIST += data/SPECS/hello2cp.spec
EXTRA_DIST += data/SPECS/hello2ln.spec
EXTRA_DIST += data/SPECS/hello2-suid.spec
EXTRA_DIST += data/SPECS/hello-g3.spec
EXTRA_DIST += data/SPECS/foo.spec
EXTRA_DIST += data/SPECS/globtest.spec
EXTRA_DIST += data/SPECS/globesctest.spec
EXTRA_DIST += data/SPECS/versiontest.spec
EXTRA_DIST += data/SPECS/conflicttest.spec
EXTRA_DIST += data/SPECS/configtest.spec
EXTRA_DIST += data/SPECS/filedep.spec
EXTRA_DIST += data/SPECS/flangtest.spec
EXTRA_DIST += data/SPECS/hlinktest.spec
EXTRA_DIST += data/SPECS/hlbreak.spec
EXTRA_DIST += data/SPECS/iftest.spec
EXTRA_DIST += data/SPECS/ifmultiline.spec
EXTRA_DIST += data/SPECS/eliftest.spec
EXTRA_DIST += data/SPECS/symlinktest.spec
EXTRA_DIST += data/SPECS/deptest.spec
EXTRA_DIST += data/SPECS/verifyscript.spec
EXTRA_DIST += data/SPECS/fakeshell.spec
EXTRA_DIST += data/SPECS/mini.spec
EXTRA_DIST += data/SPECS/scripts.spec
EXTRA_DIST += data/SPECS/scriptfail.spec
EXTRA_DIST += data/SPECS/scriptfile.spec
EXTRA_DIST += data/SPECS/selfconflict.spec
EXTRA_DIST += data/SPECS/shebang.spec
EXTRA_DIST += data/SPECS/specstep.spec
EXTRA_DIST += data/SPECS/suicidal.spec
EXTRA_DIST += data/SPECS/reloc.spec
EXTRA_DIST += data/SPECS/replacetest.spec
EXTRA_DIST += data/SPECS/triggers.spec
EXTRA_DIST += data/SPECS/filetriggers.spec
EXTRA_DIST += data/SPECS/parallel.spec
EXTRA_DIST += data/SPECS/prefixtest.spec
EXTRA_DIST += data/SPECS/testdoc.spec
EXTRA_DIST += data/SPECS/sigpipe.spec
EXTRA_DIST += data/SPECS/test-parsebits.spec
EXTRA_DIST += data/SPECS/test-subpackages.spec
EXTRA_DIST += data/SPECS/test-subpackages-exclude.spec
EXTRA_DIST += data/SPECS/test-subpackages-pathpostfixes.spec
EXTRA_DIST += data/SPECS/vattrtest.spec
EXTRA_DIST += data/SPECS/weirdnames.spec
EXTRA_DIST += data/SOURCES/buildrequires-1.0.tar.gz
EXTRA_DIST += data/SOURCES/hello-1.0-modernize.patch
EXTRA_DIST += data/SOURCES/hello-1.0-install.patch
EXTRA_DIST += data/SOURCES/hello-1.0.tar.gz
EXTRA_DIST += data/SOURCES/hello-2.0.tar.gz
EXTRA_DIST += data/RPMS/capstest-1.0-1.noarch.rpm
EXTRA_DIST += data/RPMS/foo-1.0-1.noarch.rpm
EXTRA_DIST += data/RPMS/hello-1.0-1.i386.rpm
EXTRA_DIST += data/RPMS/hello-1.0-1.ppc64.rpm
EXTRA_DIST += data/RPMS/hello-2.0-1.i686.rpm
EXTRA_DIST += data/RPMS/hello-2.0-1.x86_64.rpm
EXTRA_DIST += data/RPMS/hello-2.0-1.x86_64-signed.rpm
EXTRA_DIST += data/RPMS/hello-2.0-1.x86_64-signed-with-subkey.rpm
EXTRA_DIST += data/RPMS/hlinktest-1.0-1.noarch.rpm
EXTRA_DIST += data/RPMS/imatest-1.0-1.fc34.noarch.rpm
EXTRA_DIST += data/RPMS/hello-2.0-1.x86_64-corrupted.rpm
EXTRA_DIST += data/SRPMS/foo-1.0-1.src.rpm
EXTRA_DIST += data/SRPMS/hello-1.0-1.src.rpm
EXTRA_DIST += data/SOURCES/hello.c
EXTRA_DIST += data/SPECS/hello-attr-buildid.spec
EXTRA_DIST += data/SPECS/hello-config-buildid.spec
EXTRA_DIST += data/SPECS/hello-cd.spec
EXTRA_DIST += data/keys/rpm.org-rsa-2048-test.pub
EXTRA_DIST += data/keys/rpm.org-rsa-2048-test.secret
EXTRA_DIST += data/keys/CVE-2021-3521-badbind.asc
EXTRA_DIST += data/keys/CVE-2021-3521-nosubsig.asc
EXTRA_DIST += data/keys/CVE-2021-3521-nosubsig-last.asc
EXTRA_DIST += data/keys/different-creation-times.asc
EXTRA_DIST += data/keys/different-creation-times.secret
EXTRA_DIST += data/keys/alice.asc
EXTRA_DIST += data/keys/alice-expired-subkey.asc
EXTRA_DIST += data/keys/alice-revoked-subkey.asc
EXTRA_DIST += data/keys/alice.secret
EXTRA_DIST += data/macros.testfile
EXTRA_DIST += data/macros.debug
EXTRA_DIST += data/t1.lua
EXTRA_DIST += data/SOURCES/foo.c
EXTRA_DIST += data/SOURCES/bar.c
EXTRA_DIST += data/SOURCES/baz.c
EXTRA_DIST += data/SOURCES/foobar.h
EXTRA_DIST += data/SOURCES/libhello.c
EXTRA_DIST += data/misc/hello.intro
EXTRA_DIST += data/misc/hello.payload
EXTRA_DIST += data/misc/helloexe
EXTRA_DIST += data/misc/hellopie
EXTRA_DIST += data/misc/libhello.so

.PHONY: populate_testing

check_PROGRAMS = rpmpgpcheck

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
	  -e "s,[@]usrbindir[@],$(bindir)," \
	  -e "s,[@]usrlibdir[@],$(libdir)," \
	  -e "s,[@]execprefix[@],$(exec_prefix)," \
	  -e "s,[@]usrlibexecdir[@],$(rpmlibexecdir),g" \
	  -e "s,[@]RPMCONFIGDIR[@],$(rpmconfigdir)," \
	  -e "s,[@]PYTHON[@],$(PYTHON)," \
	  -e "s,[@]CRYPTO[@],$(CRYPTO)," \
	  -e "s,[@]__FIND_DEBUGINFO[@],$(__FIND_DEBUGINFO)," \
	< $(srcdir)/atlocal.in > atlocal
DISTCLEANFILES = atlocal
EXTRA_DIST += atlocal.in

check_DATA = atconfig atlocal $(TESTSUITE)

# The chmod is needed when copying from read-only sources (eg in distcheck)
populate_testing: $(check_DATA)
	if [ -d testing ]; then chmod -R u+w testing/; fi
	rm -rf testing
	mkdir -p testing/$(bindir)
	ln -s ./$(bindir) testing/bin
	mkdir -p testing/usr
	ln -s ../bin testing/usr/bin
	(cd ${top_builddir} && \
	          $(MAKE) DESTDIR=`pwd`/${subdir}/testing install)
	cp -r ${srcdir}/data/ testing/
	for d in dev etc magic tmp var; do if [ ! -d testing/$${d} ]; then mkdir testing/$${d}; fi; done
	for node in urandom stdin stderr stdout null full; do ln -s /dev/$${node} testing/dev/$${node}; done
	for cf in hosts resolv.conf passwd shadow group gshadow mtab ; do [ -f /etc/$${cf} ] && ln -s /etc/$${cf} testing/etc/$${cf}; done
	for prog in gzip cat cp patch tar sh ln chmod rm mkdir uname grep sed find file ionice mktemp nice cut sort diff touch install wc coreutils xargs; do p=`which $${prog}`; if [ "$${p}" != "" ]; then ln -s $${p} testing/$(bindir)/; fi; done
	for d in /proc /sys /selinux /etc/selinux; do if [ -d $${d} ]; then ln -s $${d} testing/$${d}; fi; done
	(cd testing/magic && file -C)
	chmod -R u-w testing/

if HAVE_FAKECHROOT
check-local: $(check_DATA) populate_testing
	$(SHELL) '$(TESTSUITE)' $(TESTSUITEFLAGS)
else
check-local:
	echo "you need to have fakechroot installed"
	exit 1
endif !HAVE_FAKECHROOT

installcheck-local: $(check_DATA) populate_testing
	$(SHELL) '$(TESTSUITE)' AUTOTEST_PATH='$(bindir):$(rpmlibexecdir)' $(TESTSUITEFLAGS)

clean-local:
	test ! -f '$(TESTSUITE)' || $(SHELL) '$(TESTSUITE)' --clean
	rm -f *.tmp
	if [ -d testing ]; then chmod -R u+w testing/; fi
	rm -rf testing
