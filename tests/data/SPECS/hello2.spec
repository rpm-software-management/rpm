Summary: hello2 -- double hello, world rpm
Name: hello2
Version: 1.0
Release: 1
Group: Utilities
License: GPL
Distribution: RPM test suite.
URL: http://rpm.org
Source0: hello-1.0.tar.gz
Patch0: hello-1.0-modernize.patch
Prefix: /usr

%description
Simple rpm demonstration.

%prep
%setup -q -n hello-1.0
%patch -p1 -b .modernize -P0

%build
make CFLAGS="-g -O1"
mv hello hello2
make CFLAGS="-g -O2 -D_FORTIFY_SOURCE=2"

%install
mkdir -p $RPM_BUILD_ROOT/usr/local/bin
make DESTDIR=$RPM_BUILD_ROOT install
cp hello2 $RPM_BUILD_ROOT/usr/local/bin/

%files
%defattr(-,root,root)
%doc	FAQ
#%readme README
#%license COPYING
%attr(0751,root,root)	/usr/local/bin/hello
%attr(0751,root,root)	/usr/local/bin/hello2

%changelog
* Wed May 18 2016 Mark Wielaard <mjw@redhat.com>
- Add hello2 for dwz testing support.

* Tue Oct 20 1998 Jeff Johnson <jbj@redhat.com>
- create.
