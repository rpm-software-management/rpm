%define	with_python_subpackage	0%{nil}
%define	with_python_version	2.2%{nil}
%define with_perl_subpackage	0
%define	with_bzip2		1%{nil}
%define	with_apidocs		0%{nil}
%define with_internal_db	1%{nil}
%define strip_binaries		1

# XXX enable at your own risk, CDB access to rpmdb isn't cooked yet.
%define	enable_cdb		create cdb

# XXX legacy requires './' payload prefix to be omitted from rpm packages.
%define	_noPayloadPrefix	1

%define	__prefix	/usr
%{expand: %%define __share %(if [ -d %{__prefix}/share/man ]; then echo /share ; else echo %%{nil} ; fi)}

Summary: The Red Hat package management system.
Name: rpm
%define version 4.1
Version: %{version}
%{expand: %%define rpm_version %{version}}
Release: 0.02
Group: System Environment/Base
Source: ftp://ftp.rpm.org/pub/rpm/dist/rpm-4.0.x/rpm-%{rpm_version}.tar.gz
Copyright: GPL
Conflicts: patch < 2.5
%ifos linux
Prereq: gawk fileutils textutils mktemp shadow-utils
%endif
Requires: popt = 1.7

%if !%{with_internal_db}
BuildRequires: db3-devel

# XXX glibc-2.1.92 has incompatible locale changes that affect statically
# XXX linked binaries like /bin/rpm.
%ifnarch ia64
Requires: glibc >= 2.1.92
%endif
%endif

BuildRequires: zlib-devel
# XXX Red Hat 5.2 has not bzip2 or python
%if %{with_bzip2}
BuildRequires: bzip2 >= 0.9.0c-2
%endif
%if %{with_python_subpackage}
BuildRequires: python-devel >= %{with_python_version}
%endif
%if %{with_perl_subpackage}
BuildRequires: perl >= 0:5.00503
%endif

BuildRoot: %{_tmppath}/%{name}-root

%description
The RPM Package Manager (RPM) is a powerful command line driven
package management system capable of installing, uninstalling,
verifying, querying, and updating software packages.  Each software
package consists of an archive of files along with information about
the package like its version, a description, etc.

%package devel
Summary: Development files for applications which will manipulate RPM packages.
Group: Development/Libraries
Requires: rpm = %{rpm_version}, popt = 1.7

%description devel
This package contains the RPM C library and header files.  These
development files will simplify the process of writing programs which
manipulate RPM packages and databases. These files are intended to
simplify the process of creating graphical package managers or any
other tools that need an intimate knowledge of RPM packages in order
to function.

This package should be installed if you want to develop programs that
will manipulate RPM packages and databases.

%package build
Summary: Scripts and executable programs used to build packages.
Group: Development/Tools
Requires: rpm = %{rpm_version}

%description build
This package contains scripts and executable programs that are used to
build packages using RPM.

%if %{with_python_subpackage}
%package python
Summary: Python bindings for apps which will manipulate RPM packages.
Group: Development/Libraries
Requires: rpm = %{rpm_version}
Requires: python >= %{with_python_version}
Requires: popt = 1.7

%description python
The rpm-python package contains a module which permits applications
written in the Python programming language to use the interface
supplied by RPM (RPM Package Manager) libraries.

This package should be installed if you want to develop Python
programs that will manipulate RPM packages and databases.
%endif

%if %{with_perl_subpackage}
%package perl
Summary: Native bindings to the RPM API for Perl.
Group: Development/Languages
URL: http://www.cpan.org
Provides: perl(RPM::Database) = %{rpm_version}
Provides: perl(RPM::Header) = %{rpm_version}
Requires: rpm = %{rpm_version}
Requires: perl >= 0:5.00503
Requires: popt = 1.7
Obsoletes: perl-Perl-RPM

%description perl
The Perl-RPM module is an attempt to provide Perl-level access to the
complete application programming interface that is a part of the Red
Hat Package Manager (RPM). Rather than have scripts rely on executing
RPM commands and parse the resulting output, this module aims to give
Perl programmers the ability to do anything that would otherwise have
been done in C or C++.

The interface is being designed and laid out as a collection of
classes, at least some of which are also available as tied-hash
implementations.

At this time, the interface only provides access to the database of
installed packages, and header data retrieval for RPM and SRPM files
is not yet installed.  Error management and the export of most defined
constants, through RPM::Error and RPM::Constants, respectively, are
also available.

%endif

%package -n popt
Summary: A C library for parsing command line parameters.
Group: Development/Libraries
Version: 1.7

%description -n popt
Popt is a C library for parsing command line parameters.  Popt was
heavily influenced by the getopt() and getopt_long() functions, but it
improves on them by allowing more powerful argument expansion.  Popt
can parse arbitrary argv[] style arrays and automatically set
variables based on command line arguments.  Popt allows command line
arguments to be aliased via configuration files and includes utility
functions for parsing arbitrary strings into argv[] arrays using
shell-like rules.

Install popt if you're a C programmer and you'd like to use its
capabilities.

%prep
%setup -q

%build

%ifos linux
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{__prefix} --sysconfdir=/etc --localstatedir=/var --infodir='${prefix}%{__share}/info' --mandir='${prefix}%{__share}/man'
%else
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{__prefix}
%endif

# XXX workaround ia64 gcc-3.1-0.18 miscompilation
%ifarch ia64
make CFLAGS="-g -O0 -DIA64_SUCKS_ROCKS" files.o files.lo -C build
%endif

make

%if %{with_perl_subpackage}
{ cd Perl-RPM
  CFLAGS="$RPM_OPT_FLAGS" perl Makefile.PL
  export SUBDIR="%{_builddir}/%{buildsubdir}"
  make INC="-I. -I$SUBDIR/lib -I$SUBDIR/rpmdb -I$SUBDIR/rpmio -I$SUBDIR/popt" \
    LDDLFLAGS="-shared -L$SUBDIR/lib/.libs -L$SUBDIR/rpmdb/.libs -L$SUBDIR/rpmio/.libs -L$SUBDIR/popt/.libs" %{?_smp_mflags}
}
%endif

%install
rm -rf $RPM_BUILD_ROOT

make DESTDIR="$RPM_BUILD_ROOT" install

%ifos linux

# Save list of packages through cron
mkdir -p ${RPM_BUILD_ROOT}/etc/cron.daily
install -m 755 scripts/rpm.daily ${RPM_BUILD_ROOT}/etc/cron.daily/rpm

mkdir -p ${RPM_BUILD_ROOT}/etc/logrotate.d
install -m 644 scripts/rpm.log ${RPM_BUILD_ROOT}/etc/logrotate.d/rpm

mkdir -p $RPM_BUILD_ROOT/etc/rpm
cat << E_O_F > $RPM_BUILD_ROOT/etc/rpm/macros.db1
%%_dbapi		1
E_O_F
cat << E_O_F > $RPM_BUILD_ROOT/etc/rpm/macros.cdb
%{?enable_cdb:#%%__dbi_cdb	%{enable_cdb}}
E_O_F

mkdir -p $RPM_BUILD_ROOT/var/lib/rpm
for dbi in \
	Basenames Conflictname Dirnames Group Installtid Name Providename \
	Provideversion Removetid Requirename Requireversion Triggername \
	Sigmd5 Sha1header Filemd5s Packages \
	__db.001 __db.002 __db.003 __db.004 __db.005 __db.006 __db.007 \
	__db.008 __db.009
do
    touch $RPM_BUILD_ROOT/var/lib/rpm/$dbi
done

%endif

%if %{with_apidocs}
gzip -9n apidocs/man/man*/* || :
%endif

%if %{with_perl_subpackage}
{ cd Perl-RPM
  eval `perl '-V:installsitearch'`
  eval `perl '-V:installarchlib'`
  mkdir -p $RPM_BUILD_ROOT/$installarchlib
  make PREFIX=${RPM_BUILD_ROOT}%{__prefix} \
    INSTALLMAN1DIR=${RPM_BUILD_ROOT}%{__prefix}%{__share}/man/man1 \
    INSTALLMAN3DIR=${RPM_BUILD_ROOT}%{__prefix}%{__share}/man/man3 \
	install
  rm -f $RPM_BUILD_ROOT/$installarchlib/perllocal.pod
  rm -f $RPM_BUILD_ROOT/$installsitearch/auto/RPM/.packlist
  cd ..
}
%endif

%if %{strip_binaries}
{ cd $RPM_BUILD_ROOT
  %{__strip} ./bin/rpm
  %{__strip} .%{__prefix}/bin/rpm2cpio
}
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%pre
%ifos linux
if [ -f /var/lib/rpm/Packages -a -f /var/lib/rpm/packages.rpm ]; then
    echo "
You have both
	/var/lib/rpm/packages.rpm	db1 format installed package headers
	/var/lib/rpm/Packages		db3 format installed package headers
Please remove (or at least rename) one of those files, and re-install.
"
    exit 1
fi
/usr/sbin/groupadd -g 37 rpm				> /dev/null 2>&1
/usr/sbin/useradd  -r -d /var/lib/rpm -u 37 -g 37 rpm	> /dev/null 2>&1
%endif
exit 0

%post
%ifos linux
/sbin/ldconfig
if [ -f /var/lib/rpm/packages.rpm ]; then
    /bin/chown rpm.rpm /var/lib/rpm/*.rpm
elif [ -f /var/lib/rpm/Packages ]; then
    # undo db1 configuration
    rm -f /etc/rpm/macros.db1
    /bin/chown rpm.rpm /var/lib/rpm/[A-Z]*
else
    # initialize db3 database
    rm -f /etc/rpm/macros.db1
    /bin/rpm --initdb
fi
%endif
exit 0

%ifos linux
%postun
/sbin/ldconfig
if [ $1 = 0 ]; then
    /usr/sbin/userdel rpm
    /usr/sbin/groupdel rpm
fi


%post devel -p /sbin/ldconfig
%postun devel -p /sbin/ldconfig

%post -n popt -p /sbin/ldconfig
%postun -n popt -p /sbin/ldconfig
%endif

%if %{with_python_subpackage}
%post python -p /sbin/ldconfig
%postun python -p /sbin/ldconfig
%endif

%define	rpmattr		%attr(0755, rpm, rpm)

%files
%defattr(-,root,root)
%doc RPM-PGP-KEY RPM-GPG-KEY CHANGES GROUPS doc/manual/[a-z]*
%attr(0755, rpm, rpm)	/bin/rpm

%ifos linux
%config(noreplace,missingok)	/etc/cron.daily/rpm
%config(noreplace,missingok)	/etc/logrotate.d/rpm
%dir				/etc/rpm
%config(noreplace,missingok)	/etc/rpm/macros.*
%attr(0755, rpm, rpm)	%dir /var/lib/rpm

%define	rpmdbattr %attr(0644, rpm, rpm) %verify(not md5 size mtime) %ghost %config(missingok,noreplace)
%rpmdbattr	/var/lib/rpm/Basenames
%rpmdbattr	/var/lib/rpm/Conflictname
%rpmdbattr	/var/lib/rpm/__db.0*
%rpmdbattr	/var/lib/rpm/Dirnames
%rpmdbattr	/var/lib/rpm/Filemd5s
%rpmdbattr	/var/lib/rpm/Group
%rpmdbattr	/var/lib/rpm/Installtid
%rpmdbattr	/var/lib/rpm/Name
%rpmdbattr	/var/lib/rpm/Packages
%rpmdbattr	/var/lib/rpm/Providename
%rpmdbattr	/var/lib/rpm/Provideversion
%rpmdbattr	/var/lib/rpm/Removetid
%rpmdbattr	/var/lib/rpm/Requirename
%rpmdbattr	/var/lib/rpm/Requireversion
%rpmdbattr	/var/lib/rpm/Sigmd5
%rpmdbattr	/var/lib/rpm/Sha1header
%rpmdbattr	/var/lib/rpm/Triggername

%endif

%rpmattr	%{__prefix}/bin/rpm2cpio
%rpmattr	%{__prefix}/bin/gendiff
%rpmattr	%{__prefix}/bin/rpmdb
#%rpmattr	%{__prefix}/bin/rpm[eiu]
%rpmattr	%{__prefix}/bin/rpmsign
%rpmattr	%{__prefix}/bin/rpmquery
%rpmattr	%{__prefix}/bin/rpmverify

%{__prefix}/lib/librpm-4.1.so
%{__prefix}/lib/librpmdb-4.1.so
%{__prefix}/lib/librpmio-4.1.so
%{__prefix}/lib/librpmbuild-4.1.so

%attr(0755, rpm, rpm)	%dir %{__prefix}/lib/rpm
%rpmattr	%{__prefix}/lib/rpm/config.guess
%rpmattr	%{__prefix}/lib/rpm/config.sub
%rpmattr	%{__prefix}/lib/rpm/convertrpmrc.sh
%attr(0644, rpm, rpm)	%{__prefix}/lib/rpm/macros
%rpmattr	%{__prefix}/lib/rpm/mkinstalldirs
%rpmattr	%{__prefix}/lib/rpm/rpm.*
%rpmattr	%{__prefix}/lib/rpm/rpm2cpio.sh
%rpmattr	%{__prefix}/lib/rpm/rpm[deiukqv]
%attr(0644, rpm, rpm)	%{__prefix}/lib/rpm/rpmpopt*
%attr(0644, rpm, rpm)	%{__prefix}/lib/rpm/rpmrc

%ifarch i386 i486 i586 i686 athlon
%attr(-, rpm, rpm)		%{__prefix}/lib/rpm/i[3456]86*
%attr(-, rpm, rpm)		%{__prefix}/lib/rpm/athlon*
%endif
%ifarch alpha alphaev5 alphaev56 alphapca56 alphaev6 alphaev67
%attr(-, rpm, rpm)		%{__prefix}/lib/rpm/alpha*
%endif
%ifarch sparc sparcv9 sparc64
%attr(-, rpm, rpm)		%{__prefix}/lib/rpm/sparc*
%endif
%ifarch ia64
%attr(-, rpm, rpm)		%{__prefix}/lib/rpm/ia64*
%endif
%ifarch powerpc ppc ppciseries ppcpseries ppcmac
%attr(-, rpm, rpm)		%{__prefix}/lib/rpm/ppc*
%endif
%ifarch s390 s390x
%attr(-, rpm, rpm)		%{__prefix}/lib/rpm/s390*
%endif
%ifarch armv3l armv4l
%attr(-, rpm, rpm)		%{__prefix}/lib/rpm/armv[34][lb]*
%endif
%ifarch mips mipsel mipseb
%attr(-, rpm, rpm)		%{__prefix}/lib/rpm/mips*
%endif
%attr(-, rpm, rpm)		%{__prefix}/lib/rpm/noarch*

%lang(cs)	%{__prefix}/*/locale/cs/LC_MESSAGES/rpm.mo
%lang(da)	%{__prefix}/*/locale/da/LC_MESSAGES/rpm.mo
%lang(de)	%{__prefix}/*/locale/de/LC_MESSAGES/rpm.mo
%lang(fi)	%{__prefix}/*/locale/fi/LC_MESSAGES/rpm.mo
%lang(fr)	%{__prefix}/*/locale/fr/LC_MESSAGES/rpm.mo
%lang(is)	%{__prefix}/*/locale/is/LC_MESSAGES/rpm.mo
%lang(ja)	%{__prefix}/*/locale/ja/LC_MESSAGES/rpm.mo
%lang(ko)	%{__prefix}/*/locale/ko/LC_MESSAGES/rpm.mo
%lang(no)	%{__prefix}/*/locale/no/LC_MESSAGES/rpm.mo
%lang(pl)	%{__prefix}/*/locale/pl/LC_MESSAGES/rpm.mo
%lang(pt)	%{__prefix}/*/locale/pt/LC_MESSAGES/rpm.mo
%lang(pt_BR)	%{__prefix}/*/locale/pt_BR/LC_MESSAGES/rpm.mo
%lang(ro)	%{__prefix}/*/locale/ro/LC_MESSAGES/rpm.mo
%lang(ru)	%{__prefix}/*/locale/ru/LC_MESSAGES/rpm.mo
%lang(sk)	%{__prefix}/*/locale/sk/LC_MESSAGES/rpm.mo
%lang(sl)	%{__prefix}/*/locale/sl/LC_MESSAGES/rpm.mo
%lang(sr)	%{__prefix}/*/locale/sr/LC_MESSAGES/rpm.mo
%lang(sv)	%{__prefix}/*/locale/sv/LC_MESSAGES/rpm.mo
%lang(tr)	%{__prefix}/*/locale/tr/LC_MESSAGES/rpm.mo

%{__prefix}%{__share}/man/man1/gendiff.1*
%{__prefix}%{__share}/man/man8/rpm.8*
%{__prefix}%{__share}/man/man8/rpm2cpio.8*
%lang(pl)	%{__prefix}%{__share}/man/pl/man[18]/*.[18]*
%lang(ru)	%{__prefix}%{__share}/man/ru/man[18]/*.[18]*
%lang(sk)	%{__prefix}%{__share}/man/sk/man[18]/*.[18]*

%files build
%defattr(-,root,root)
%dir %{__prefix}/src/redhat
%dir %{__prefix}/src/redhat/BUILD
%dir %{__prefix}/src/redhat/SPECS
%dir %{__prefix}/src/redhat/SOURCES
%dir %{__prefix}/src/redhat/SRPMS
%dir %{__prefix}/src/redhat/RPMS
%{__prefix}/src/redhat/RPMS/*
%rpmattr	%{__prefix}/bin/rpmbuild
%rpmattr	%{__prefix}/lib/rpm/brp-*
%rpmattr	%{__prefix}/lib/rpm/check-prereqs
%rpmattr	%{__prefix}/lib/rpm/config.site
%rpmattr	%{__prefix}/lib/rpm/cpanflute
%rpmattr	%{__prefix}/lib/rpm/cpanflute2
%rpmattr	%{__prefix}/lib/rpm/cross-build
%rpmattr	%{__prefix}/lib/rpm/find-lang.sh
%rpmattr	%{__prefix}/lib/rpm/find-prov.pl
%rpmattr	%{__prefix}/lib/rpm/find-provides
%rpmattr	%{__prefix}/lib/rpm/find-provides.perl
%rpmattr	%{__prefix}/lib/rpm/find-req.pl
%rpmattr	%{__prefix}/lib/rpm/find-requires
%rpmattr	%{__prefix}/lib/rpm/find-requires.perl
%rpmattr	%{__prefix}/lib/rpm/get_magic.pl
%rpmattr	%{__prefix}/lib/rpm/getpo.sh
%rpmattr	%{__prefix}/lib/rpm/http.req
%rpmattr	%{__prefix}/lib/rpm/javadeps
%rpmattr	%{__prefix}/lib/rpm/magic.prov
%rpmattr	%{__prefix}/lib/rpm/magic.req
%rpmattr	%{__prefix}/lib/rpm/perl.prov
%rpmattr	%{__prefix}/lib/rpm/Specfile.pm

# XXX remove executable bit to disable autogenerated perl requires for now.
%rpmattr	%{__prefix}/lib/rpm/perl.req
#%attr(0644, rpm, rpm) %{__prefix}/lib/rpm/perl.req

%rpmattr	%{__prefix}/lib/rpm/rpm[bt]
%rpmattr	%{__prefix}/lib/rpm/rpmdiff
%rpmattr	%{__prefix}/lib/rpm/rpmdiff.cgi
%rpmattr	%{__prefix}/lib/rpm/trpm
%rpmattr	%{__prefix}/lib/rpm/u_pkg.sh
%rpmattr	%{__prefix}/lib/rpm/vpkg-provides.sh
%rpmattr	%{__prefix}/lib/rpm/vpkg-provides2.sh

%{__prefix}%{__share}/man/man8/rpmbuild.8*

%if %{with_python_subpackage}
%files python
%defattr(-,root,root)
%{__prefix}/lib/python%{with_python_version}/site-packages/rpmmodule.so
#%{__prefix}/lib/python%{with_python_version}/site-packages/poptmodule.so
%endif

%if %{with_perl_subpackage}
%files perl
%defattr(-,root,root)
%rpmattr	%{__prefix}/bin/rpmprune
%{perl_sitearch}/auto/*
%{perl_sitearch}/RPM
%{perl_sitearch}/RPM.pm
%{__prefix}%{__share}/man/man1/rpmprune.1*
%{__prefix}%{__share}/man/man3/RPM*
%endif

%files devel
%defattr(-,root,root)
%if %{with_apidocs}
%doc 
%endif
%{__prefix}/include/rpm
%{__prefix}/lib/librpm.a
%{__prefix}/lib/librpm.la
%{__prefix}/lib/librpm.so
%{__prefix}/lib/librpmdb.a
%{__prefix}/lib/librpmdb.la
%{__prefix}/lib/librpmdb.so
%{__prefix}/lib/librpmio.a
%{__prefix}/lib/librpmio.la
%{__prefix}/lib/librpmio.so
%{__prefix}/lib/librpmbuild.a
%{__prefix}/lib/librpmbuild.la
%{__prefix}/lib/librpmbuild.so

%files -n popt
%defattr(-,root,root)
%{__prefix}/lib/libpopt.so.*
%{__prefix}%{__share}/man/man3/popt.3*
%lang(cs)	%{__prefix}/*/locale/cs/LC_MESSAGES/popt.mo
%lang(da)	%{__prefix}/*/locale/da/LC_MESSAGES/popt.mo
%lang(gl)	%{__prefix}/*/locale/gl/LC_MESSAGES/popt.mo
%lang(hu)	%{__prefix}/*/locale/hu/LC_MESSAGES/popt.mo
%lang(is)	%{__prefix}/*/locale/is/LC_MESSAGES/popt.mo
%lang(ko)	%{__prefix}/*/locale/ko/LC_MESSAGES/popt.mo
%lang(no)	%{__prefix}/*/locale/no/LC_MESSAGES/popt.mo
%lang(pt)	%{__prefix}/*/locale/pt/LC_MESSAGES/popt.mo
%lang(ro)	%{__prefix}/*/locale/ro/LC_MESSAGES/popt.mo
%lang(ru)	%{__prefix}/*/locale/ru/LC_MESSAGES/popt.mo
%lang(sk)	%{__prefix}/*/locale/sk/LC_MESSAGES/popt.mo
%lang(sl)	%{__prefix}/*/locale/sl/LC_MESSAGES/popt.mo
%lang(sv)	%{__prefix}/*/locale/sv/LC_MESSAGES/popt.mo
%lang(tr)	%{__prefix}/*/locale/tr/LC_MESSAGES/popt.mo
%lang(uk)	%{__prefix}/*/locale/uk/LC_MESSAGES/popt.mo
%lang(wa)	%{__prefix}/*/locale/wa/LC_MESSAGES/popt.mo
%lang(zh_CN)	%{__prefix}/*/locale/zh_CN.GB2312/LC_MESSAGES/popt.mo

# XXX These may end up in popt-devel but it hardly seems worth the effort now.
%{__prefix}/lib/libpopt.a
%{__prefix}/lib/libpopt.la
%{__prefix}/lib/libpopt.so
%{__prefix}/include/popt.h

%changelog
* Sun Mar 10 2002 Jeff Johnson <jbj@redhat.com>
- make --addsign and --resign behave exactly the same.
- splint annotationsm, signature cleanup.

* Mon Sep 24 2001 Jeff Johnson <jbj@redhat.com>
- Start rpm-4.1.
- loosely wire beecrypt library into rpm.
- drop rpmio/base64.[ch] in favor of beecrypt versions.
- drop lib/md5*.[ch] files in favor of beecrypt.
- legacy: drop brokenMD5 support (rpm-2.3.3 to rpm-2.3.8 on sparc).
- eliminate DYING code.
- bind beecrypt md5/sha1 underneath rpmio.
- create RFC-2440 OpenPGP API in rpmio.
- proof-of-concept GPG/DSA verification for legacy signatures.
- upgrade to beecrypt-2.2.0pre.
- proof-of-concept PGP/RSA verification for legacy signatures.
- ratchet up to lclint "strict" level.
- upgrade to db-4.0.7.
- use only header methods, routines are now static.
- beecrypt is at least as good as pgp/gpg on verify, pulling the plug.
- add :base64 and :armor format extensions, dump binary tags in hex.
- proof-of-concept pubkey retrieval from RPM-{PGP,GPG}-KEY.
- stupid macros to configure public key file paths.
- all symbols but hdrVec are now forward references in linkage.
- generate an rpm header on the fly for imported pubkeys.
- wire transactions through rpmcli signature modes.
- wire transactions through rpmcli query/verify modes.
- wire transactions through rpmcli install/erase modes.
- legacy signatures always checked where possible on package read.
- wire transactions through rpmcli build modes.
- lazy rpmdb open/close through transaction methods (mostly anyways).
- no-brainer refcounts for rpmdb object.
- check added header against transaction set, replace if newer.
- transaction sets created in cli main.
- no-brainer refcounts for ts object.
- memory indices for dependency check are typedef'd and abstract'd.
- no-brainer refcounts for fi object, debug the mess.
- dump the header early in transaction, recreate fi before installing.
- start hiding availablePackage data/methods in rpmal.c/rpmal.h.
- add some dinky availablePackage methods.
- transaction.c: cleanly uncouple availablePackage from TFI_t.
- add header refcount annotations throughout.
- depends.c: availablePackage is (almost) opaque.
- invent some toy transactionElement iterators.
- create rpmDepSet constructors/destructors.
- create toy rpmDepSet iterators.
- rpmRangesOverlap renamed to dsCompare, add dsNotify method as well.
- depends.c: rpmDepSet is (almost) opaque, move to rpmds.[ch].
- rpmds: create dsProblem(), dsiGetDNEVR() retrieved DNEVR, not N.
- depends.h: hack around teIterator() et al from include for now.
- rpmds: move trigger dependencies into a rpmDepSet as well.
- rpmal: availablePackage is totally opaque, alKey with index replaces.
- fix: harmless typo in db3 chroot hack.
- fix: big-endian's with sizeof(time_t) != sizeof(int_32) mtime broken.
- fix: add Korean message catalogs (#54473).
- add RPHNPLATFORM and PLATFORM tags.
- linear search on added package provides is dumb.
- discarding entire signature header when using --addsign is dumb.
- rip out rpmDependencyConflict, replace with rpmProblem instead.
- no-brainer refcounts for rpmProblemSet object.
- header tag sets are per-transactionElement, not per-availablePackage.
- no-brainer refcounts for rpmDepSet and rpmFNSet objects.
- strip header tags for erased as well as installed transactionElements.
- common structure elements for unification of TFI_t and rpmFNSet.
- factor per-transactionElement data out of TFI_t through pointer ref.
- unify rpmFNSet into TFI_t.
- eliminate header reference in rpmtransAddPackage, use TFI_t data.
- commit to using rpmDepSet and TFI_t, not header.
- lclint rpmio fiddles.
- split file info tag sets into rpmfi.c.
- create toy TFI_t iterators.
- tweak overlapped file fingerprint retrieval for speed.
- transaction.c: use wrappers/iterators to access TFI_t.
- annotations to make a transactionElement opaque.
- use array of pointers rather than contiguous array for ts->order.
- methods to complete making transactionElement opaque.
- use TR_REMOVED relations as well as TR_ADDED for ordering.
- drop requirement that removed packages immediately follow added.
- hybrid chainsaw/presentation ordering algorithm.
- convert file md5sum's to binary on the fly, reducing memory footprint.
- header handling moved to librpmdb to avoid linkage loops.
- fix a couple dinky memory leaks.
- build with an internal zlib for now.
- protect brp-compress against /bin/ls output ambiguity (#56656,#56336).
- 3 madvise calls and a 16Mb mmapped buffer == ~5% install speedup. Wow.
- use db-4.0.14 final internally.
- 1st crack at making zlib rsync friendly.
- lclint-3.0.0.19 fiddles.
- solaris: translate i86pc to i386 (#57182).
- fix: %%GNUconfigure breaks with single quotes (#57264).
- simple automake wrapper for zlib.
- add buildarch lines for hppa (#57728).
- sparc: make dbenv per-rpmdb, not per-dbi.
- handle lazy db open's in chroot with absolute path, not prefix strip.
- Depends should use CDB if configured.
- autodetect python 1.5/2.2.
- make rpm-perl package self-hosting (#57748).
- permit gpg/pgp/pgp5 execs to be reconfigured.
- fix: signing multiple times dinna work, discard immutable region.
- remove poptmodule.so for separate packaging.
- permit subset installs for %%lang colored hardlink file sets.
- missing key(s) on keyring when verifying a signature is now an error.
- remove dependency whiteout.
- splint fiddles.
- ppc64 arch added (#58634,#58636).
- turn on auto-generated perl requirements (#58519, #58536, #58537).
- fix: %%exclude functional (again).
- trap SIGILL for ppc64 inline asm voodoo fix from cross-dressed ppc32.
- rpm-perl: force numeric comparison on rpm version (#58882).
- fix: fancy hash fiddles if not a tty.
- fix: handle /.../ correctly in rpmCleanPath().
- legacy: configurable whiteout for known Red Hat dependency loops.
- perl.req: don't mis-generate dependencies like perl(::path/to/foo.pl).
- permit args to be hidden within %%__find_{requires,provides}.
- a couple more perl.{prov,req} fiddles.
- macro for %%files, always include %%defattr(), redhat config only.
- fix: drop header region when writing repackaged legacy header.
- bail on %%files macro.
- transaction rollbacks are functional.
- generate index for pkgid (aka Sigmd5) tag, query/verify by pkgid.
- generate index for hdrid (aka Sha1header) tag, query/verify by hdrid.
- generate index for fileid (aka Filemd5s) tag, query/verify by fileid.
- query/verify by install transaction id.
- rpm-4.0.4 release candidate.
- add cpanflute2, another perl.req fiddle.
- make peace with gcc-3.1, remove compiler cruft.
- make peace with automake et al in 8.0, ugh.
- add payload uncompressed size to signature to avoid rewriting header.
- drill header sha1 into signature parallel to header+payload md5.
- mandatory "most effective" signature check on query/verify/install.
- don't bother adding empty filemd's to index.
- add Pubkey index, using signer id as binary key.
- display pubkeys in hex when debugging db access.
- retrieve pubkey(s) from rpmdb, not from detached signature file.
- reapply Berkeley DB patch #4491.
- add header DSA signature.
- add header RSA signature (untested, disabled for now).
- don't bother with signing check if 16 bits of hash don't match.
- only V3 signatures for now.
- wire --nodigest/--nosignature options to checksig/query/verify modes.
