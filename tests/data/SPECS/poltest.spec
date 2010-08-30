%define debug_package %{nil}

Summary: Policy in rpm example
Name:    poltest
Version: 1.0
Release: 2
Group:   Utilities
License: GPL
Requires: poltest-policy
Source0: poltest-%{version}.tar.bz2
Source1: poltest-policy-%{version}.tar.bz2
Buildroot: %{_tmppath}/%{name}-%{version}-root
%description
Example for installing policy included in a package header

%prep
%setup -q
%setup -q -T -D -a 1

%build
make CFLAGS="$RPM_OPT_FLAGS"
make -f /usr/share/selinux/devel/Makefile -C poltest-policy-%{version}

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=%{buildroot} prefix=%{_prefix} install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_bindir}/foo
%{_bindir}/bar


%package policy
Summary: Policy for the poltest package
Group: System/Policy
Collections: sepolicy
%description policy
Policy for the poltest package

%sepolicy policy
%semodule -n foo -t default      poltest-policy-%{version}/foo.pp
%semodule -n bar -t mls,targeted poltest-policy-%{version}/bar.pp

%files policy

%changelog
* Wed Jul 1 2009 Steve Lawrence <slawrence@tresys.com>
- create
