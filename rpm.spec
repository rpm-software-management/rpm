%define	__prefix	/usr
%{expand:%%define __share %(if [ -d %{__prefix}/share/man ]; then echo /share ; else echo %%{nil} ; fi)}

Summary: The Red Hat package management system.
Name: rpm
%define version 4.0
Version: %{version}
Release: 0.26
Group: System Environment/Base
Source: ftp://ftp.rpm.org/pub/rpm/dist/rpm-3.0.x/rpm-%{version}.tar.gz
Copyright: GPL
Conflicts: patch < 2.5
%ifos linux
Prereq: gawk fileutils textutils sh-utils mktemp
Requires: popt, bzip2 >= 0.9.0c-2
BuildRequires: db3-devel
BuildRequires: bzip2 >= 0.9.0c-2
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

%ifos linux
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
Version: 1.6

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

%ifos linux
make -C python
%endif

%install
rm -rf $RPM_BUILD_ROOT

make DESTDIR="$RPM_BUILD_ROOT" install
%ifos linux
make DESTDIR="$RPM_BUILD_ROOT" install -C python
%endif
mkdir -p $RPM_BUILD_ROOT/etc/rpm

{ cd $RPM_BUILD_ROOT
  strip ./bin/rpm
  strip .%{__prefix}/bin/rpm2cpio
  strip .%{__prefix}/lib/rpm/rpmputtext .%{__prefix}/lib/rpm/rpmgettext
}

%clean
rm -rf $RPM_BUILD_ROOT

%post
%ifos linux
if [ ! -e /etc/rpm/macros -a -e /etc/rpmrc -a -f %{__prefix}/lib/rpm/convertrpmrc.sh ] 
then
	sh %{__prefix}/lib/rpm/convertrpmrc.sh > /dev/null 2>&1
fi
%else
/bin/rpm --initdb
%endif

%ifos linux
%post devel -p /sbin/ldconfig
%postun devel -p /sbin/ldconfig

%post -n popt -p /sbin/ldconfig
%postun -n popt -p /sbin/ldconfig
%endif

%ifos linux
%post python -p /sbin/ldconfig
%postun python -p /sbin/ldconfig
%endif

%files
%defattr(-,root,root)
%doc RPM-PGP-KEY RPM-GPG-KEY CHANGES GROUPS doc/manual/*
/bin/rpm
%dir /etc/rpm
%{__prefix}/bin/rpm2cpio
%{__prefix}/bin/gendiff
%{__prefix}/bin/rpmdb
%{__prefix}/bin/rpm[eiukqv]
%{__prefix}/lib/librpm.so.*
%{__prefix}/lib/librpmbuild.so.*

%{__prefix}/lib/rpm/brp-*
%{__prefix}/lib/rpm/config.guess
%{__prefix}/lib/rpm/config.sub
%{__prefix}/lib/rpm/convertrpmrc.sh
%{__prefix}/lib/rpm/find-prov.pl
%{__prefix}/lib/rpm/find-provides
%{__prefix}/lib/rpm/find-req.pl
%{__prefix}/lib/rpm/find-requires
%{__prefix}/lib/rpm/freshen.sh
%{__prefix}/lib/rpm/macros
%{__prefix}/lib/rpm/mkinstalldirs
%{__prefix}/lib/rpm/rpmdb
%{__prefix}/lib/rpm/rpm[eiukqv]
%{__prefix}/lib/rpm/rpmpopt
%{__prefix}/lib/rpm/rpmrc
%{__prefix}/lib/rpm/vpkg-provides.sh
%{__prefix}/lib/rpm/vpkg-provides2.sh
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
%{__prefix}/lib/rpm/powerpc*
%endif

%dir %{__prefix}/src/redhat
%dir %{__prefix}/src/redhat/BUILD
%dir %{__prefix}/src/redhat/SPECS
%dir %{__prefix}/src/redhat/SOURCES
%dir %{__prefix}/src/redhat/SRPMS
%dir %{__prefix}/src/redhat/RPMS
%{__prefix}/src/redhat/RPMS/*
%{__prefix}/*/locale/*/LC_MESSAGES/rpm.mo
%{__prefix}%{__share}/man/man[18]/*.[18]*
%lang(pl) %{__prefix}%{__share}/man/pl/man[18]/*.[18]*
%lang(ru) %{__prefix}%{__share}/man/ru/man[18]/*.[18]*

%files build
%defattr(-,root,root)
%{__prefix}/bin/rpm[bt]
%{__prefix}/lib/rpm/check-prereqs
%{__prefix}/lib/rpm/cpanflute
%{__prefix}/lib/rpm/find-lang.sh
%{__prefix}/lib/rpm/find-provides.perl
%{__prefix}/lib/rpm/find-requires.perl
%{__prefix}/lib/rpm/get_magic.pl
%{__prefix}/lib/rpm/getpo.sh
%{__prefix}/lib/rpm/http.req
%{__prefix}/lib/rpm/magic.prov
%{__prefix}/lib/rpm/magic.req
%{__prefix}/lib/rpm/perl.prov
%{__prefix}/lib/rpm/perl.req
%{__prefix}/lib/rpm/rpm[bt]
%{__prefix}/lib/rpm/rpmdiff
%{__prefix}/lib/rpm/rpmdiff.cgi
%{__prefix}/lib/rpm/rpmgettext
%{__prefix}/lib/rpm/rpmputtext
%{__prefix}/lib/rpm/u_pkg.sh

%ifos linux
%files python
%defattr(-,root,root)
%{__prefix}/lib/python1.5/site-packages/rpmmodule.so
%endif

%files devel
%defattr(-,root,root)
%{__prefix}/include/rpm
%{__prefix}/lib/librpm.a
%{__prefix}/lib/librpm.la
%{__prefix}/lib/librpm.so
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
* Tue May 30 2000 Matt Wilson <msw@redhat.com>
- fix configure and makeinstall macros

* Tue May 30 2000 Jeff Johnson <jbj@redhat.com>
- mark packaging with version 4 to reflect filename/provide changes.
- change next version from 3.1 to 4.0 to reflect package format change.

* Wed May 26 2000 Jeff Johnson <jbj@redhat.com>
- change popt exec alias in oreder to exec rpm children.
- split rpm into 5 pieces along major mode fault lines with popt glue.

* Thu May 18 2000 Jeff Johnson <jbj@redhat.com>
- 2nd try at db1 -> db3 stable functionality.

* Tue May 16 2000 Matt Wilson <msw@redhat.com>
- build against bzip2 1.0
- use the new fopencookie API in glibc 2.2

* Fri May 12 2000 Jeff Johnson <jbj@redhat.com>
- fix stupid mistakes (alpha segfaults).

* Wed May 10 2000 Jeff Johnson <jbj@redhat.com>
- include RPM-GPG-KEY in file manifest.
- simplify --last popt alias, date like -qi (bjerrick@easystreet.com).
- fix: alloca'd memory used outside of scope (alpha segfault).

* Mon May  8 2000 Jeff Johnson <jbj@redhat.com>
- FreeBSD fixes (bero@redhat.com).

* Sat May  6 2000 Jeff Johnson <jbj@redhat.com>
- finish db1 and db3 cleanup.

* Tue May  2 2000 Jeff Johnson <jbj@redhat.com>
- first try at db1 -> db3 stability.

* Mon May  1 2000 Jeff Johnson <jbj@redhat.com>
- Rename db0.c to db1.c, resurrect db2.c (from db3.c).
- Add ia64 and sparc64 changes.
- rpm.spec: add per-platform sub-directories.

* Fri Apr 28 2000 Jeff Johnson <jbj@redhat.com>
- Filter DB_INCOMPLETE on db->sync, it's usually harmless.
- Add per-transaction cache of resolved dependencies (aka Depends).
- Do lazy dbi{Open,Close} throughout.
- Attempt fine grained dbi cursors throughout.
- fix: free iterator *after* loop, not during.
- fix: Depends needed keylen in dbiPut, rpmdbFreeIterator after use.

* Thu Apr 27 2000 Jeff Johnson <jbj@redhat.com>
- API: replace rpmdbUpdateRecord with rpmdbSetIteratorModified.
- API: replace rpmdbFindByLabel with RPMDBI_LABEL iteration.
- API: replace rpmdbGetRecord with iterators.
- API: replace findMatches with iterators.

* Tue Apr 25 2000 Jeff Johnson <jbj@redhat.com>
- rebuild to check autoconf configuration in dist-7.0.

* Sun Apr 23 2000 Jeff Johnson <jbj@redhat.com>
- fix: cpio.c: pre-, not post-, decrement the link count.
- make db indices as lightweight as possible, with per-dbi config.
- db1.c will never be needed, eliminate.
- API: merge rebuilddb.c into rpmdb.c.

* Thu Apr 13 2000 Jeff Johnson <jbj@redhat.com>
- API: pass *SearchIndex() length of key (0 will use strlen(key)).
- API: remove rpmdb{First,Next}RecNum routines.
- drop rpm-python subpackage until bindings are fixed.
- add explcit "Provides: name = [epoch:]version-release" to headers.

* Tue Apr 11 2000 Jeff Johnson <jbj@redhat.com>
- solaris2.6: avoid bsearch with empty dir list (Ric Klaren - klaren@cs.utwente.nl)
- db3: save join keys in endian neutral binary format.
- treat legacy falloc.c as "special" db[01] index for package headers.

* Thu Apr  6 2000 Jeff Johnson <jbj@redhat.com>
- use hashed access for package headers.

* Tue Apr  4 2000 Jeff Johnson <jbj@redhat.com>
- create dbi from template rather than passed args.

* Mon Apr  3 2000 Jeff Johnson <jbj@redhat.com>
- prefer db3 as default.
- permit db3 configuration using macros.

* Fri Mar 31 2000 Jeff Johnson <jbj@redhat.com>
- try for db3 DB_INIT_CDB model.

* Fri Mar 24 2000 Jeff Johnson <jbj@redhat.com>
- use DIRNAMES/BASENAMES/DIRINDICES not FILENAMES in packages and db.
- configure.in fiddles for BSD systems (Patrick Schoo).
- API: change dbi to pass by reference, not value.
- cram all of db1, db_185, and db2 interfaces into rpmlib.
- convert db1 -> db2 on-disk format using --rebuilddb.

* Mon Mar 13 2000 Jeff Johnson <jbj@redhat.com>
- start rpm-3.1 development.
