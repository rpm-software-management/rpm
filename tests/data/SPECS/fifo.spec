Name: fifo
Version: 1.0
Release: 1
Group: Testing
License: GPL
Summary: Testing fifo behavior
BuildArch: noarch

%description
%{summary}

%install
mknod ${RPM_BUILD_ROOT}/test-fifo p

%files
/test-fifo
