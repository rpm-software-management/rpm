Summary: The Red Hat package management system.
Name: rpm
%define version 3.0.4
Version: %{version}
Release: 0.31
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
The Red Hat Package Manager (RPM) is a powerful command line driven
package management system capable of installing, uninstalling,
verifying, querying, and updating software packages.  Each software
package consists of an archive of files along with information about
the package like its version, a description, etc.

%package devel
Summary: Development files for applications which will manipulate RPM packages.
Group: Development/Libraries
Requires: popt

%description devel
This package contains the RPM C library and header files.  These
development files will simplify the process of writing programs which
manipulate RPM packages and databases. These file are intended to
simplify the process of creating graphical package managers or any
other tools that need an intimate knowledge of RPM packages in order
to function.

This package should be installed if you want to develop programs that
will manipulate RPM packages and databases.

%ifos linux
%package python
Summary: Python bindings for applications which will manipulate RPM packages.
Group: Development/Libraries
BuildRequires: popt >= 1.5
Requires: popt >= 1.5
Requires: python >= 1.5.2

%description python
This package contains the module that permits Python applications to use
the interface supplied by RPM libraries.

This package should be installed if you want to develop Python programs that
will manipulate RPM packages and databases.
%endif

%package -n popt
Summary: A C library for parsing command line parameters.
Group: System Environment/Libraries
Version: 1.5

%description -n popt
Popt is a C library for parsing command line parameters.  Popt
was heavily influenced by the getopt() and getopt_long() functions,
but it improves on them by allowing more powerful argument expansion.
Popt can parse arbitrary argv[] style arrays and automatically set
variables based on command line arguments.  Popt allows command
line arguments to be aliased via configuration files and includes
utility functions for parsing arbitrary strings into argv[] arrays
using shell-like rules.

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
	sh /usr/lib/rpm/convertrpmrc.sh 2>&1 > /dev/null
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
/usr/bin/rpm2cpio
/usr/bin/gendiff
/usr/lib/librpm.so.*
/usr/lib/librpmbuild.so.*
/usr/lib/rpm
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
