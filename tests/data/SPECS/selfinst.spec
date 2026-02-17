Name: selfinst
Version: 1
Release: 1
License: Public domain
Summary: Testing install from transaction
BuildArch: noarch

%description

%pretrans
echo PRETRANS
rpm -Uv /build/RPMS/noarch/selfinst-1-1.noarch.rpm
echo $?

%posttrans
echo POSTTRANS
rpm -Uv /build/RPMS/noarch/selfinst-1-1.noarch.rpm
echo $?

%files
