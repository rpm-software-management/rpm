Name:           test
Version:        1.0
Release:        1
Summary:        Test

License:        Public Domain
URL:            https://fedoraproject.org
Source:         hello.c

%description
%{summary}.

%prep
%autosetup -c -D -T
cp -a %{S:0} .

%build
gcc -g hello.c -o hello

%install
mkdir -p %{buildroot}%{_bindir}
install -D -p -m 0755 -t %{buildroot}%{_bindir} hello

%files
%attr(644,root,root) %{_bindir}/hello

%changelog
