Summary: foo
Name: foo
Version: 1.0
Release: 1
Source: hello-2.0.tar.gz
Patch1: hello-1.0-modernize.patch
Group: Testing
License: GPLv2+
BuildArch: noarch

%description
Simple rpm demonstration.

%package sub
Summary: %{summary}
Requires: %{name} = %{version}-%{release}

%description sub
%{summary}

%files

%files sub
