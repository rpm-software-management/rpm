Summary: The zlib compression and decompression library.
Name: zlib
Version: 1.1.3
Release: 24
Group: System Environment/Libraries
Source: ftp://ftp.info-zip.org/pub/infozip/zlib/zlib-%{version}.tar.gz
Patch0: zlib-1.1.3-glibc.patch
URL: http://www.gzip.org/zlib/
License: BSD
Prefix: %{_prefix}
BuildRoot: %{_tmppath}/%{name}-%{version}-root

%description
Zlib is a general-purpose, patent-free, lossless data compression
library which is used by many different programs.

%package devel
Summary: Header files and libraries for Zlib development.
Group: Development/Libraries
Requires: zlib = %{version}

%description devel
The zlib-devel package contains the header files and libraries needed
to develop programs that use the zlib compression and decompression
library.

%prep
%setup -q
%patch0 -p1 -b .glibc

%build


CFLAGS="$RPM_OPT_FLAGS -fPIC" ./configure --shared --prefix=%{_prefix}
make
# now build the static lib
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{_prefix}
make

%install
rm -rf ${RPM_BUILD_ROOT}
mkdir -p ${RPM_BUILD_ROOT}%{_prefix}

CFLAGS="$RPM_OPT_FLAGS" ./configure --shared --prefix=%{_prefix}
make install prefix=${RPM_BUILD_ROOT}%{_prefix}

CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{_prefix}
make install prefix=${RPM_BUILD_ROOT}%{_prefix}

install -m644 zutil.h ${RPM_BUILD_ROOT}%{_includedir}/zutil.h
mkdir -p ${RPM_BUILD_ROOT}%{_mandir}/man3
install -m644 zlib.3 ${RPM_BUILD_ROOT}%{_mandir}/man3

if [ "%{_prefix}/lib" != "%{_libdir}" ]; then
    mkdir -p ${RPM_BUILD_ROOT}%{_libdir}
    mv ${RPM_BUILD_ROOT}%{_prefix}/lib/* ${RPM_BUILD_ROOT}%{_libdir}
    rmdir ${RPM_BUILD_ROOT}%{_prefix}/lib
fi

%clean
rm -rf ${RPM_BUILD_ROOT}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root)
%doc README
%{_libdir}/libz.so.*

%files devel
%defattr(-,root,root)
%doc ChangeLog algorithm.txt minigzip.c example.c
%{_libdir}/*.a
%{_libdir}/*.so
%{_includedir}/*
%{_mandir}/man3/zlib.3*

%changelog
* Sun Aug 26 2001 Trond Eivind Glomsrød <teg@redhat.com> 1.1.3-24
- Add example.c and minigzip.c to the doc files, as
  they are listed as examples in the README (#52574)

* Mon Jun 18 2001 Trond Eivind Glomsrød <teg@redhat.com>
- Updated URL
- Add version dependency for zlib-devel
- s/Copyright/License/

* Wed Feb 14 2001 Trond Eivind Glomsrød <teg@redhat.com>
- bumped version number - this is the old version without the performance enhancements

* Fri Sep 15 2000 Florian La Roche <Florian.LaRoche@redhat.de>
- add -fPIC for shared libs (patch by Fritz Elfert)

* Thu Sep  7 2000 Jeff Johnson <jbj@redhat.com>
- on 64bit systems, make sure libraries are located correctly.

* Thu Aug 17 2000 Jeff Johnson <jbj@redhat.com>
- summaries from specspo.

* Thu Jul 13 2000 Prospector <bugzilla@redhat.com>
- automatic rebuild

* Sun Jul 02 2000 Trond Eivind Glomsrød <teg@redhat.com>
- rebuild

* Tue Jun 13 2000 Jeff Johnson <jbj@redhat.com>
- FHS packaging to build on solaris2.5.1.

* Wed Jun 07 2000 Trond Eivind Glomsrød <teg@redhat.com>
- use %%{_mandir} and %%{_tmppath}

* Fri May 12 2000 Trond Eivind Glomsrød <teg@redhat.com>
- updated URL and source location
- moved README to main package

* Mon Feb  7 2000 Jeff Johnson <jbj@redhat.com>
- compress man page.

* Sun Mar 21 1999 Cristian Gafton <gafton@redhat.com> 
- auto rebuild in the new build environment (release 5)

* Wed Sep 09 1998 Cristian Gafton <gafton@redhat.com>
- link against glibc

* Mon Jul 27 1998 Jeff Johnson <jbj@redhat.com>
- upgrade to 1.1.3

* Fri May 08 1998 Prospector System <bugs@redhat.com>
- translations modified for de, fr, tr

* Wed Apr 08 1998 Cristian Gafton <gafton@redhat.com>
- upgraded to 1.1.2
- buildroot

* Tue Oct 07 1997 Donnie Barnes <djb@redhat.com>
- added URL tag (down at the moment so it may not be correct)
- made zlib-devel require zlib

* Thu Jun 19 1997 Erik Troan <ewt@redhat.com>
- built against glibc
