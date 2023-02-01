Summary: libhello -- hello, world rpm
Name: libhello
Version: 1.0.1
Release: 1
Group: Utilities
License: GPL
Distribution: RPM test suite.
URL: http://rpm.org
Source0: libhello.c

%description
Simple rpm demonstration.

%prep
%setup -q -c -T

%build
gcc -DIAM_LIB -shared -fPIC -Wl,-soname,libhello.so %{SOURCE0} -o libhello.so

%install
mkdir -p $RPM_BUILD_ROOT/usr/local/lib64
install -m 0755 libhello.so $RPM_BUILD_ROOT/usr/local/lib64/libhello.so

%files
%defattr(-,root,root)
/usr/local/lib64/libhello.so

%changelog
* Mon Jan 05 2026 Gordon Messmer <gmessmer@redhat.com>
- libhello rpm for elfdep tests
