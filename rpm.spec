Summary: Red Hat Package Manager
Name: rpm
%define version 2.90
Version: %{version}
Release: 7
Group: Utilities/System
Source: ftp://ftp.rpm.org/pub/rpm/dist/rpm-2.5.x/rpm-%{version}.tar.gz
Copyright: GPL
BuildRoot: /var/tmp/rpm-%{version}-root
Conflicts: patch < 2.5

%description
RPM is a powerful package manager, which can be used to build, install, 
query, verify, update, and uninstall individual software packages. A 
package consists of an archive of files, and package information, including 
name, version, and description.

%package devel
Summary: Header files and libraries for programs that manipulate rpm packages
Group: Development/Libraries

%description devel
The RPM packaging system includes a C library that makes it easy to
manipulate RPM packages and databases. It is intended to ease the
creation of graphical package managers and other tools that need
intimate knowledge of RPM packages.

%prep
%setup -q

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=/usr
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

%clean
rm -rf $RPM_BUILD_ROOT

%post
/bin/rpm --initdb

%files
%defattr(-,root,root)
%doc RPM-PGP-KEY CHANGES groups
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
