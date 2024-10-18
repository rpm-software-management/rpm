Name: pamupdate
Version: 1.0
Release: %{rel}
Group: Testing
License: Public domain
Summary: Test config behavior on update failure
BuildArch: noarch

%description
%{summary}

%install
mkdir -p ${RPM_BUILD_ROOT}/etc
echo AAAA > ${RPM_BUILD_ROOT}/etc/my.conf
echo BBBB > ${RPM_BUILD_ROOT}/etc/your.conf
mkdir -p ${RPM_BUILD_ROOT}/var/run/faillock

%files
%config /etc/my.conf
%config /etc/your.conf
%dir /var/run/faillock
