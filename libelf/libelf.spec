Summary: An ELF object file access shared library.
Name: libelf
Version: 0.7.0
Release: 5
Copyright: distributable
Group: System Environment/Libraries
Source: ftp://www.ibiblio.org/pub/Linux/libs/libelf-0.7.0.tar.gz
Patch: libelf-0.7.0.patch
Patch2: libelf-0.7.0-hash.patch
Buildroot: /var/tmp/libelf-root

%description
The libelf package contains a shared library for accessing ELF object files.
You need to install it if you use any tools linked against it dynamically.

%package devel
Summary: An ELF object file access library.
Group: Development/Libraries
Requires: libelf = %{version}-%{release}

%description devel
The libelf-devel package contains a library for accessing ELF object
files. Libelf allows you to access the internals of the ELF object file
format, so you can see the different sections of an ELF file.

%prep
%setup
%patch -p1
%patch2 -p1

%build
%{__libtoolize} --copy --force
%configure --enable-shared
make

%install
%makeinstall

%files
%defattr(-,root,root)
%doc README
%{_prefix}/%{_lib}/libelf.so.*

%files devel
%defattr(-,root,root)
%{_prefix}/%{_lib}/libelf.so
%{_prefix}/%{_lib}/libelf.a
%{_prefix}/include/libelf

%changelog
* Mon Jun 17 2002 Jakub Jelinek <jakub@redhat.com> 0.7.0-5
- build libelf shared and split into libelf and libelf-devel
  subpackages (#66184)
- don't override SHT_HASH's sh_entsize for 64-bit ELF,
  some platforms like Alpha use 64-bit hash entries

* Thu May 23 2002 Tim Powers <timp@redhat.com>
- automated rebuild

* Wed Jan 09 2002 Tim Powers <timp@redhat.com>
- automated rebuild

* Wed Sep 26 2001 Jakub Jelinek <jakub@redhat.com> 0.7.0-2
- fix Elf64_Sxword conversion

* Tue Jul  3 2001 Jakub Jelinek <jakub@redhat.com> 0.7.0-1
- update to 0.7.0 to support 64bit elf

* Sun Jun 24 2001 Elliot Lee <sopwith@redhat.com>
- Bump release + rebuild.

* Tue Jul 18 2000 Nalin Dahyabhai <nalin@redhat.com>
- add %%defattr (release 7)

* Thu Jul 13 2000 Prospector <bugzilla@redhat.com>
- automatic rebuild

* Sun Jun 18 2000 Matt Wilson <msw@redhat.com>
- rebuilt for next release
- don't build on ia64 (libelf don't grok 64bit elf)

* Sun Mar 21 1999 Cristian Gafton <gafton@redhat.com> 
- auto rebuild in the new build environment (release 4)

* Thu Jan 14 1999 Cristian Gafton <gafton@redhat.com>
- build for glibc 2.1

* Fri May 01 1998 Prospector System <bugs@redhat.com>
- translations modified for de, fr, tr

* Fri Oct 31 1997 Michael K. Johnson <johnsonm@redhat.com>
- upgraded to 0.6.4
- buildroot

