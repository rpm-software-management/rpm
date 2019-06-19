Summary: hello-g3 -- double hello, world rpm, .debug_macro -g3
Name: hello-g3
Version: 1.0
Release: 1
Group: Testing
License: GPL
Source0: hello-1.0.tar.gz
Patch0: hello-1.0-modernize.patch

%description
Simple rpm demonstration.

%prep
%setup -q -n hello-1.0
%patch0 -p1 -b .modernize

%build
make CFLAGS="-g3 -O1 -DDEBUG_DEFINE=1"
mv hello hello-g3
make CFLAGS="-g3 -O2 -D_FORTIFY_SOURCE=2"

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/local/bin
make DESTDIR=$RPM_BUILD_ROOT install
cp hello-g3 $RPM_BUILD_ROOT/usr/local/bin/

%files
%defattr(-,root,root)
%doc	FAQ
%attr(0751,root,root)	/usr/local/bin/hello
%attr(0751,root,root)	/usr/local/bin/hello-g3

%changelog
* Mon Jun  3 2019 Mark Wielaard <mjw@fedoraproject.org>
- Create hello-g3 for -g3 .debug_macro testing.

* Wed May 18 2016 Mark Wielaard <mjw@redhat.com>
- Add hello2 for dwz testing support.
