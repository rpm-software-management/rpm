Summary: The Red Hat package management system.
Name: rpm
%define version 3.0.4
Version: %{version}
Release: 0.47
Group: System Environment/Base
Source: ftp://ftp.rpm.org/pub/rpm/dist/rpm-3.0.x/rpm-%{version}.tar.gz
Copyright: GPL
Conflicts: patch < 2.5
%ifos linux
Prereq: gawk fileutils textutils sh-utils mktemp
BuildRequires: bzip2 >= 0.9.0c-2
Requires: popt, bzip2 >= 0.9.0c-2
BuildRequires: python-devel >= 1.5.2
%endif
BuildRoot: /var/tmp/%{name}-root

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
Version: 1.5

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
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=/usr
make
%ifos linux
make -C python
%endif

%install
rm -rf $RPM_BUILD_ROOT

make DESTDIR="$RPM_BUILD_ROOT" install
%ifos linux
make DESTDIR="$RPM_BUILD_ROOT" install -C python
mkdir -p $RPM_BUILD_ROOT/etc/rpm
%endif

{ cd $RPM_BUILD_ROOT
  strip ./bin/rpm
  strip ./usr/bin/rpm2cpio
  strip ./usr/lib/rpm/rpmputtext ./usr/lib/rpm/rpmgettext
}

%clean
rm -rf $RPM_BUILD_ROOT

%post
/bin/rpm --initdb
%ifos linux
if [ ! -e /etc/rpm/macros -a -e /etc/rpmrc -a -f /usr/lib/rpm/convertrpmrc.sh ] 
then
	sh /usr/lib/rpm/convertrpmrc.sh > /dev/null 2>&1
fi
%endif

%ifos linux
%post devel -p /sbin/ldconfig
%postun devel -p /sbin/ldconfig

%post python -p /sbin/ldconfig
%postun python -p /sbin/ldconfig

%post -n popt -p /sbin/ldconfig
%postun -n popt -p /sbin/ldconfig
%endif

%files
%defattr(-,root,root)
%doc RPM-PGP-KEY CHANGES GROUPS doc/manual/*
/bin/rpm
%ifos linux
%dir /etc/rpm
%endif
/usr/bin/rpm2cpio
/usr/bin/gendiff
/usr/lib/librpm.so.*
/usr/lib/librpmbuild.so.*

/usr/lib/rpm/brp-*
/usr/lib/rpm/config.guess
/usr/lib/rpm/config.sub
/usr/lib/rpm/convertrpmrc.sh
/usr/lib/rpm/find-prov.pl
/usr/lib/rpm/find-provides
/usr/lib/rpm/find-req.pl
/usr/lib/rpm/find-requires
/usr/lib/rpm/freshen.sh
/usr/lib/rpm/macros
/usr/lib/rpm/mkinstalldirs
/usr/lib/rpm/rpmpopt
/usr/lib/rpm/rpmrc
/usr/lib/rpm/vpkg-provides.sh
/usr/lib/rpm/vpkg-provides2.sh

%dir /usr/src/redhat
%dir /usr/src/redhat/BUILD
%dir /usr/src/redhat/SPECS
%dir /usr/src/redhat/SOURCES
%dir /usr/src/redhat/SRPMS
%dir /usr/src/redhat/RPMS
/usr/src/redhat/RPMS/*
/usr/share/locale/*/LC_MESSAGES/rpm.mo
/usr/man/man[18]/*.[18]*
%lang(pl) /usr/man/pl/man[18]/*.[18]*
%lang(ru) /usr/man/ru/man[18]/*.[18]*

%files build
%defattr(-,root,root)
/usr/lib/rpm/check-prereqs
/usr/lib/rpm/cpanflute
/usr/lib/rpm/find-lang.sh
/usr/lib/rpm/find-provides.perl
/usr/lib/rpm/find-requires.perl
/usr/lib/rpm/get_magic.pl
/usr/lib/rpm/getpo.sh
/usr/lib/rpm/http.req
/usr/lib/rpm/magic.prov
/usr/lib/rpm/magic.req
/usr/lib/rpm/perl.prov
/usr/lib/rpm/perl.req
/usr/lib/rpm/rpmdiff
/usr/lib/rpm/rpmdiff.cgi
/usr/lib/rpm/rpmgettext
/usr/lib/rpm/rpmputtext
/usr/lib/rpm/u_pkg.sh

%ifos linux
%files python
%defattr(-,root,root)
/usr/lib/python1.5/site-packages/rpmmodule.so
%endif

%files devel
%defattr(-,root,root)
/usr/include/rpm
/usr/lib/librpm.a
/usr/lib/librpm.la
/usr/lib/librpm.so
/usr/lib/librpmbuild.a
/usr/lib/librpmbuild.la
/usr/lib/librpmbuild.so

%files -n popt
%defattr(-,root,root)
/usr/lib/libpopt.so.*
/usr/share/locale/*/LC_MESSAGES/popt.mo
/usr/man/man3/popt.3*

# XXX These may end up in popt-devel but it hardly seems worth the effort now.
/usr/lib/libpopt.a
/usr/lib/libpopt.la
/usr/lib/libpopt.so
/usr/include/popt.h

%changelog
* Wed Mar  1 2000 Jeff Johnson <jbj@redhat.com>
- fix rpmmodule.so python bindings.

* Sun Feb 27 2000 Jeff Johnson <jbj@redhat.com>
- rpm-3.0.4 release candidate.

* Fri Feb 25 2000 Jeff Johnson <jbj@redhat.com>
- fix: filter excluded paths before adding install prefixes (#8709).
- add i18n lookaside to PO catalogue(s) for i18n strings.
- try for /etc/rpm/macros.specspo so that specspo autoconfigures rpm.
- per-platform configuration factored into /usr/lib/rpm subdir.

* Tue Feb 15 2000 Jeff Johnson <jbj@redhat.com>
- new rpm-build package to isolate rpm dependencies on perl/bash2.
- always remove duplicate identical package entries on --rebuilddb.
- add scripts for autogenerating CPAN dependencies.

* Wed Feb  9 2000 Jeff Johnson <jbj@redhat.com>
- brp-compress deals with hard links correctly.

* Mon Feb  7 2000 Jeff Johnson <jbj@redhat.com>
- brp-compress deals with symlinks correctly.

* Mon Jan 24 2000 Jeff Johnson <jbj@redhat.com>
- explicitly expand file lists in writeRPM for rpmputtext.
