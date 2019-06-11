Summary: Test dynamic BuildRequires
Name: buildrequires
Version: 1.0
Release: 1
Group: Utilities
License: GPL
Distribution: RPM test suite.
Source0: buildrequires-1.0.tar.gz
Prefix: /usr
BuildArch: noarch

%description
Simple build requires demonstration.

%prep
%setup -q

%generate_buildrequires
echo foo-bar = 2.0
cat buildrequires.txt

%build

%install
mkdir -p $RPM_BUILD_ROOT/usr/local/bin
cat < EOF >> $RPM_BUILD_ROOT/usr/local/bin/hello
#!/bin/sh
echo hello
EOF
chmod a+x $RPM_BUILD_ROOT/usr/local/bin/hello

%clean
rm -rf $RPM_BUILD_ROOT

%pre

%post

%preun

%postun

%files
%defattr(-,root,root)
%doc	FAQ
#%readme README
#%license COPYING
%attr(0751,root,root)	/usr/local/bin/hello

