Name: verifyfiles
Version: 1.0
Release: 1
Group: Testing
License: GPL
Summary: Testing verifyfiles behavior
BuildArch: noarch

%description
%summary.

%prep

%build

%install
touch %{buildroot}/test-verify1
touch %{buildroot}/test-verify2

%files
%verify(mode md5 size mtime) /test-verify1
%verify(not mode) /test-verify2
