Name: selfimport
Version: 1
Release: 1
License: Public domain
Summary: Testing keyring import from transaction
BuildArch: noarch

%description

%pretrans
echo PRETRANS
rpmkeys --import /data/keys/rpm.org-rsa-2048-test.pub; echo $?
exit 0

%posttrans
echo POSTTRANS
rpmkeys --import /data/keys/rpm.org-rsa-2048-test.pub; echo $?
exit 0

%files
