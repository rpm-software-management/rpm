Name: selfkeys
Version: 1
Release: 1
License: Public domain
Summary: Testing keyring listing from transaction
BuildArch: noarch

%description

%pretrans
echo PRETRANS
rpmkeys -l | wc -l
exit 0

%posttrans
echo POSTTRANS
rpmkeys -l | wc -l
exit 0

%files
