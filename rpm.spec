Summary: The Red Hat package management system.
Name: rpm
%define version 2.95
Version: %{version}
Release: 5
Group: System Environment/Base
Source: ftp://ftp.rpm.org/pub/rpm/dist/rpm-2.5.x/rpm-%{version}.tar.gz
Copyright: GPL
BuildRoot: /var/tmp/rpm-%{version}-root
Conflicts: patch < 2.5

%description
The Red Hat Package Manager (RPM) is a powerful command line driven
package management system capable of installing, uninstalling,
verifying, querying, and updating software packages.  Each software
package consists of an archive of files along with information about
the package like its version, a description, etc.

%package devel
Summary: Development files for applications which will manipulate RPM packages.
Group: Development/Libraries

%description devel
This package contains the RPM C library and header files.  These
development files will simplify the process of writing programs
which manipulate RPM packages and databases and are intended to make
it easier to create graphical package managers or any other tools
that need an intimate knowledge of RPM packages in order to function.

This package should be installed if you want to develop programs that
will manipulate RPM packages and databases.

%prep
%setup -q

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=/usr --disable-shared
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/lib

mkdir -p $RPM_BUILD_ROOT/usr/src/redhat/SOURCES
mkdir -p $RPM_BUILD_ROOT/usr/src/redhat/SPECS
mkdir -p $RPM_BUILD_ROOT/usr/src/redhat/SRPMS
mkdir -p $RPM_BUILD_ROOT/usr/src/redhat/BUILD
mkdir -p $RPM_BUILD_ROOT/usr/src/redhat/RPMS/${RPM_ARCH}
mkdir -p $RPM_BUILD_ROOT/usr/src/redhat/RPMS/noarch

make DESTDIR="$RPM_BUILD_ROOT" install

{ cd $RPM_BUILD_ROOT
  strip ./bin/rpm
  strip ./usr/bin/rpm2cpio
}

%clean
rm -rf $RPM_BUILD_ROOT

%post
/bin/rpm --initdb

%files
%defattr(-,root,root)
%doc RPM-PGP-KEY CHANGES GROUPS
%doc docs/*
/bin/rpm
/usr/bin/rpm2cpio
/usr/bin/gendiff
/usr/man/man8/rpm.8
/usr/man/man8/rpm2cpio.8
/usr/lib/rpm
/usr/lib/rpmrc
/usr/lib/rpmpopt
%dir /usr/src/redhat
%dir /usr/src/redhat/BUILD
%dir /usr/src/redhat/SPECS
%dir /usr/src/redhat/SOURCES
%dir /usr/src/redhat/SRPMS
%dir /usr/src/redhat/RPMS
/usr/src/redhat/RPMS/*
/usr/share/locale/*/LC_MESSAGES/rpm.mo

%files devel
%defattr(-,root,root)
/usr/include/rpm
/usr/lib/librpm.a
