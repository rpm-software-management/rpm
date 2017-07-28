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
RemovePathPostfixes: .foobar
Summary: Test2.
%description test2

%prep
%autosetup -c -D -T
cp -a %{S:0} .

%build
gcc -g hello.c -o hello
cp hello.c hello2.c
gcc -g hello2.c -o hello.foobar

%install
mkdir -p %{buildroot}/bin
install -D -p -m 0755 -t %{buildroot}/bin hello
# Install as hello.foobar, but we want the postfix removed in the package...
install -D -p -m 0755 -t %{buildroot}/bin hello.foobar

%files
/bin/hello

%files test2
/bin/hello.foobar

%changelog
