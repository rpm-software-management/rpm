Name:    test
Version: 1
Release: 1
Summary: test SOURCE_DATE_EPOCH
License: GPLv2
BuildArch: noarch

%description

%build
echo "this is a test" > 0.txt

%install
%{__install} -m 644 -D 0.txt %{buildroot}/0.txt

%files
/0.txt
