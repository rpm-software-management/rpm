%define	with_python_subpackage	1
%define	with_bzip2		1
%define	with_apidocs		1
%define strip_binaries		0
%define	__spec_install_post	:

# XXX legacy requires './' payload prefix to be omitted from rpm packages.
%define	_noPayloadPrefix	1

%define	__prefix	/usr
%{expand:%%define __share %(if [ -d %{__prefix}/share/man ]; then echo /share ; else echo %%{nil} ; fi)}

Summary: The Red Hat package management system.
Name: rpm
%define version 4.0.2
Version: %{version}
Release: 0.3
Group: System Environment/Base
Source: ftp://ftp.rpm.org/pub/rpm/dist/rpm-4.0.x/rpm-%{version}.tar.gz
Copyright: GPL
Conflicts: patch < 2.5
%ifos linux
Prereq: gawk fileutils textutils mktemp
Requires: popt
%endif

BuildRequires: db3-devel

# XXX glibc-2.1.92 has incompatible locale changes that affect statically
# XXX linked binaries like /bin/rpm.
%ifnarch ia64
Requires: glibc >= 2.1.92
# XXX needed to avoid libdb.so.2 satisfied by compat/libc5 provides.
Requires: db1 = 1.85
%endif

# XXX Red Hat 5.2 has not bzip2 or python
%if %{with_bzip2}
BuildRequires: bzip2 >= 0.9.0c-2
%endif
%if %{with_python_subpackage}
BuildRequires: python-devel >= 1.5.2
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
Requires: rpm = %{version}, popt

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
Requires: rpm = %{version}

%description build
This package contains scripts and executable programs that are used to
build packages using RPM.

%if %{with_python_subpackage}
%package python
Summary: Python bindings for apps which will manipulate RPM packages.
Group: Development/Libraries
BuildRequires: popt >= 1.5
Requires: popt >= 1.5
Requires: python >= 1.5.2

%description python
The rpm-python package contains a module which permits applications
written in the Python programming language to use the interface
supplied by RPM (RPM Package Manager) libraries.

This package should be installed if you want to develop Python
programs that will manipulate RPM packages and databases.
%endif

%package -n popt
Summary: A C library for parsing command line parameters.
Group: Development/Libraries
Version: 1.6.2

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

make

%install
rm -rf $RPM_BUILD_ROOT

make DESTDIR="$RPM_BUILD_ROOT" install

mkdir -p $RPM_BUILD_ROOT/etc/rpm
cat << E_O_F > $RPM_BUILD_ROOT/etc/rpm/macros.db1
%%_dbapi		1
E_O_F

%if %{strip_binaries}
{ cd $RPM_BUILD_ROOT
  strip ./bin/rpm
  strip .%{__prefix}/bin/rpm2cpio
}
%endif

%if %{with_apidocs}
gzip -9n apidocs/man/* || :
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%pre
if [ -f /var/lib/rpm/Packages -a -f /var/lib/rpm/packages.rpm ]; then
#    echo "
#You have both
#	/var/lib/rpm/packages.rpm	db1 format installed package headers
#	/var/lib/rpm/Packages		db3 format installed package headers
#Please remove (or at least rename) one of those files, and re-install.
#"
    exit 1
fi

%post
%ifos linux
/sbin/ldconfig
%endif
if [ -f /var/lib/rpm/packages.rpm ]; then
    : # do nothing
elif [ -f /var/lib/rpm/Packages ]; then
    # undo db1 configuration
    rm -f /etc/rpm/macros.db1
else
    # initialize db3 database
    rm -f /etc/rpm/macros.db1
    /bin/rpm --initdb
fi

%ifos linux
%postun -p /sbin/ldconfig

%post devel -p /sbin/ldconfig
%postun devel -p /sbin/ldconfig

%post -n popt -p /sbin/ldconfig
%postun -n popt -p /sbin/ldconfig
%endif

%if %{with_python_subpackage}
%post python -p /sbin/ldconfig
%postun python -p /sbin/ldconfig
%endif

%files
%defattr(-,root,root)
%doc RPM-PGP-KEY RPM-GPG-KEY CHANGES GROUPS doc/manual/[a-z]*
/bin/rpm
%dir			/etc/rpm
%config(missingok)	/etc/rpm/macros.db1
%{__prefix}/bin/rpm2cpio
%{__prefix}/bin/gendiff
%{__prefix}/bin/rpmdb
%{__prefix}/bin/rpm[eiukqv]
%{__prefix}/bin/rpmsign
%{__prefix}/bin/rpmquery
%{__prefix}/bin/rpmverify
%{__prefix}/lib/librpm.so.*
%{__prefix}/lib/librpmio.so.*
%{__prefix}/lib/librpmbuild.so.*

%{__prefix}/lib/rpm/config.guess
%{__prefix}/lib/rpm/config.sub
%{__prefix}/lib/rpm/convertrpmrc.sh
%{__prefix}/lib/rpm/macros
%{__prefix}/lib/rpm/mkinstalldirs
%{__prefix}/lib/rpm/rpmdb
%{__prefix}/lib/rpm/rpm[eiukqv]
%{__prefix}/lib/rpm/rpmpopt*
%{__prefix}/lib/rpm/rpmrc

%ifarch i386 i486 i586 i686
%{__prefix}/lib/rpm/i[3456]86*
%endif
%ifarch alpha
%{__prefix}/lib/rpm/alpha*
%endif
%ifarch sparc sparc64
%{__prefix}/lib/rpm/sparc*
%endif
%ifarch ia64
%{__prefix}/lib/rpm/ia64*
%endif
%ifarch powerpc ppc
%{__prefix}/lib/rpm/ppc*
%endif
%ifarch armv3l armv4l
%{__prefix}/lib/rpm/armv[34][lb]*
%endif

%{__prefix}/*/locale/*/LC_MESSAGES/rpm.mo
%{__prefix}%{__share}/man/man[18]/*.[18]*
%lang(pl) %{__prefix}%{__share}/man/pl/man[18]/*.[18]*
%lang(ru) %{__prefix}%{__share}/man/ru/man[18]/*.[18]*
%lang(sk) %{__prefix}%{__share}/man/sk/man[18]/*.[18]*

%files build
%defattr(-,root,root)
%dir %{__prefix}/src/redhat
%dir %{__prefix}/src/redhat/BUILD
%dir %{__prefix}/src/redhat/SPECS
%dir %{__prefix}/src/redhat/SOURCES
%dir %{__prefix}/src/redhat/SRPMS
%dir %{__prefix}/src/redhat/RPMS
%{__prefix}/src/redhat/RPMS/*
%{__prefix}/bin/rpmbuild
%{__prefix}/lib/rpm/brp-*
%{__prefix}/lib/rpm/check-prereqs
%{__prefix}/lib/rpm/cpanflute
%{__prefix}/lib/rpm/find-lang.sh
%{__prefix}/lib/rpm/find-prov.pl
%{__prefix}/lib/rpm/find-provides
%{__prefix}/lib/rpm/find-provides.perl
%{__prefix}/lib/rpm/find-req.pl
%{__prefix}/lib/rpm/find-requires
%{__prefix}/lib/rpm/find-requires.perl
%{__prefix}/lib/rpm/get_magic.pl
%{__prefix}/lib/rpm/getpo.sh
%{__prefix}/lib/rpm/http.req
%{__prefix}/lib/rpm/javadeps
%{__prefix}/lib/rpm/magic.prov
%{__prefix}/lib/rpm/magic.req
%{__prefix}/lib/rpm/perl.prov
%{__prefix}/lib/rpm/perl.req
%{__prefix}/lib/rpm/rpm[bt]
%{__prefix}/lib/rpm/rpmdiff
%{__prefix}/lib/rpm/rpmdiff.cgi
%{__prefix}/lib/rpm/u_pkg.sh
%{__prefix}/lib/rpm/vpkg-provides.sh
%{__prefix}/lib/rpm/vpkg-provides2.sh

%if %{with_python_subpackage}
%files python
%defattr(-,root,root)
%{__prefix}/lib/python1.5/site-packages/rpmmodule.so
%endif

%files devel
%defattr(-,root,root)
%if %{with_apidocs}
%doc apidocs
%endif
%{__prefix}/include/rpm
%{__prefix}/lib/librpm.a
%{__prefix}/lib/librpm.la
%{__prefix}/lib/librpm.so
%{__prefix}/lib/librpmio.a
%{__prefix}/lib/librpmio.la
%{__prefix}/lib/librpmio.so
%{__prefix}/lib/librpmbuild.a
%{__prefix}/lib/librpmbuild.la
%{__prefix}/lib/librpmbuild.so

%files -n popt
%defattr(-,root,root)
%{__prefix}/lib/libpopt.so.*
%{__prefix}/*/locale/*/LC_MESSAGES/popt.mo
%{__prefix}%{__share}/man/man3/popt.3*

# XXX These may end up in popt-devel but it hardly seems worth the effort now.
%{__prefix}/lib/libpopt.a
%{__prefix}/lib/libpopt.la
%{__prefix}/lib/libpopt.so
%{__prefix}/include/popt.h

%changelog
* Sat Dec 16 2000 Jeff Johnson <jbj@redhat.com>
- gendiff: generate ChangeLog patches more intelligently (#22356).

* Wed Dec 13 2000 Jeff Johnson <jbj@redhat.com>
- bump popt version.
- fix: (transaction.c) assume file state normal if tag is missing.
- fix: failed signature read headerFree segfault.
- fix: revert ALPHA_LOSSAGE, breaks 6.2/i386.
- fix: segfault on build path, ignore deleted drips.
- fix: synthesized callbacks for removed packages have not a pkgkey.

* Tue Dec 12 2000 Jeff Johnson <jbj@redhat.com>
- bail on header regions.
- change dependency loop message to RPMMESS_WARNING to use stderr, not stdout.

* Sun Dec 10 2000 Jeff Johnson <jbj@redhat.com>
- handle added dirtoken tags (mostly) correctly with header regions.
- add FHS doc/man/info dirs, diddle autoconf goo.
- fix: headerUnload handles headers w/o regions correctly on rebuilddb.

* Thu Dec  7 2000 Jeff Johnson <jbj@redhat.com>
- add rpmtransGetKeys() to retrieve transaction keys in tsort'ed order.
- python bindings for rpmtransGetKeys().
- fix: include alignment in count when swabbing header region.

* Wed Dec  6 2000 Jeff Johnson <jbj@redhat.com>
- improved find-{requires,provides} for aix4/hpux/irix6/osf.
		Tim Mooney<mooney@dogbert.cc.ndsu.NoDak.edu>
- portability: remove use of GNU make subst in lib/Makefile (Joe Orton).
- python: bind package removal (#21274).
- autoconfigure building python bindings.
- autoconfigure generating rpm API doco rpm-devel package.
- fix: don't fdFree in rpmVerifyScript, rpmtransFree does already.
- unify rpmError and rpmMessge interfaces through rpmlog.
- collect and display rpm build error messages at end of build.
- use package version 3 if --nodirtokens is specified.
- add package names to problem sets early, don't save removed header.
- make sure that replaced tags in region are counted in headerSizeof().
- support for dmalloc debugging.
- filter region tags in headerNextIterator, exit throut headerReload.

* Thu Nov 30 2000 Jeff Johnson <jbj@redhat.com>
- add missing headerFree for legacy signature header.
- fix: removed packages leaked DIRINDEXES tag data.
- reload tags added during install when loading header from rpmdb.
- avoid brp-compress hang with both compressed/uncompressed man pages.

* Tue Nov 21 2000 Jeff Johnson <jbj@redhat.com>
- add brp-strip-shared script <rodrigob@conectiva.com.br>.
- better item/task progress bars <rodrigob@conectiva.com.br>.
- load headers as single contiguous region.
- add region marker as RPM_BIN_TYPE in packages and database.
- fix: don't headerCopy() relocateable packages if not relocating.
- merge signatures into header after reading from package.

* Mon Nov 20 2000 Jeff Johnson <jbj@redhat.com>
- add doxygen and lclint annotations most everywhere.
- consistent return for all signature verification.
- use enums for almost all rpmlib #define's.
- API: change rpmProblem typedef to pass by reference, not value.
- don't trim leading ./ in rpmCleanPath() (#14961).
- detect (still need to test) rdonly linux file systems.
- check available inodes as well as blocks on mounted file systems.
- pass rpmTransactionSet, not elements, to installBinaryPackage et al.
- add cscope/ctags (Rodrigo Barbosa<rodrigob@conectiva.com.br>).
- remove getMacroBody() from rpmio API.
- add support for unzip <rodrigob@conectiva.com.br>

* Thu Nov 16 2000 Jeff Johnson <jbj@redhat.com>
- don't verify src rpm MD5 sums (yet).
- md5 sums are little endian (no swap) so big endian needs the swap.

* Wed Nov 15 2000 Jeff Johnson <jbj@redhat.com>
- fix: segfault on exit of "rpm -qp --qf '%{#fsnames}' pkg" (#20595).
- hpux w/o -D_OPEN_SOURCE has not h_errno.
- verify MD5 sums of payload files when unpacking archive.
- hide libio lossage in prototype, not API.
- add support for SHA1 as well as MD5 message digests.

* Mon Nov 13 2000 Jeff Johnson <jbj@redhat.com>
- fix: work around for (mis-compilation?!) segfaults on signature paths.

* Sun Nov 12 2000 Jeff Johnson <jbj@redhat.com>
- fix: duplicate headerFree() on instalBinaryPackage() error return.

* Sat Nov 11 2000 Jeff Johnson <jbj@redhat.com>
- fix: runTriggers was not adding countCorrection.
- add rpmGetRpmlibProvides() to retrieve rpmlib(...) provides
	"Pawel A. Gajda" <mis@k2.net.pl>.
- syntax to specify source of Requires: (PreReq: now legacy).
- rip out rpm{get,put}text, use getpo.sh and specspo instead.
- fine-grained Requires, remove install PreReq's from Requires db.

* Wed Oct 11 2000 Jeff Johnson <jbj@redhat.com>
- fix: rpm2cpio error check wrong on non-libio platforms.

* Fri Sep 29 2000 Jeff Johnson <jbj@redhat.com>
- fix: more (possible) xstrdup side effects.

* Wed Sep 27 2000 Jeff Johnson <jbj@redhat.com>
- bump popt version to 1.6.1.

* Tue Sep 26 2000 Jeff Johnson <jbj@redhat.com>
- fix: avoid calling getpass twice as side effect of xstrdup macro (#17672).
- order packages using tsort, clipping PreReq:'s in dependency loops.
- handle possible db3 dependency on -lpthread more gracefully.

* Thu Sep 14 2000 Jeff Johnson <jbj@redhat.com>
- start rpm-4.0.1.
