Name:           test
Epoch:          1
Version:        1.0
Release:        1
Summary:        Test

License:        Public Domain
URL:            https://fedoraproject.org
Source:         hello.c

%description
%{summary}.

%package test2
Summary: Test2.
%description test2

%package test3
Epoch:   0
Summary: Test3.
%description test3

%files
/bin/hello

%files test2
/bin/hello2

%files test3
/bin/hello3

%changelog
