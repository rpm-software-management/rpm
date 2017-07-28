Name:           test
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
Summary: Test3.
%description test3

%prep
%autosetup -c -D -T
cp -a %{S:0} .

%build
gcc -g hello.c -o hello
cp hello.c hello2.c
gcc -g hello2.c -o hello2
cp hello.c hello3.c
gcc -g hello3.c -o hello3

%install
mkdir -p %{buildroot}/bin
install -D -p -m 0755 -t %{buildroot}/bin hello
install -D -p -m 0755 -t %{buildroot}/bin hello2
install -D -p -m 0755 -t %{buildroot}/bin hello3

%files
/bin/hello

%files test2
/bin/hello2

%files test3
/bin/hello3

%changelog
