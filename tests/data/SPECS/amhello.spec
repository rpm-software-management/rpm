Name: amhello
Version: 1.0
Release: 1
Source: %{name}-%{version}.tar.gz
License: GPLv2
Summary: Autotools example
Autobuild: autotools

%description
%{summary}

%build -a
cat << EOF > README.distro
Add some distro specific notes.
EOF

%files
%doc README.distro
%{_bindir}/hello
%{_docdir}/%{name}
