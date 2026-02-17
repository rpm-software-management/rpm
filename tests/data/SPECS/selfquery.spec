Name: selfquery
Version: 1
Release: 1
License: Public domain
Summary: Testing query from transaction
BuildArch: noarch

%description

%pretrans
echo PRETRANS
rpm -q %{name}
exit 0

%posttrans
echo POSTTRANS
rpm -q %{name}
exit 0

%files
