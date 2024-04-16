Name: noperms
Version: 1.0
Release: 1
License: Public Domain
Summary: Test read-only files in build
BuildArch: noarch

%description
%{summary}

%prep
%setup -c -T

%build
mkdir bad
touch bad/foo
chmod a-w bad

%install
cp -avp . ${RPM_BUILD_ROOT}

%files
/bad
