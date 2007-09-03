Summary: foo
Name: foo
Version: 1.0
Release: 1
Group: Utilities
License: GPLv2+
Distribution: RPM test suite.
Provides: hi
Conflicts: goodbye
Obsoletes: howdy
BuildArch: noarch
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
Simple rpm demonstration.

%prep

%build

%install

%clean
rm -rf $RPM_BUILD_ROOT

%files
