%bcond_with alt

%{!?buildsys:%global buildsys autotools}
%{!?srcname:%global srcname amhello-1.0.tar.gz}

Name: amhello
Version: 1.0
Source: %{srcname}
License: GPLv2
Summary: Autotools example
BuildSystem: %{buildsys}

%if %{with alt}
BuildOption: --program-prefix=alt-
BuildOption(install): DESTDIR=${RPM_BUILD_ROOT}/alt
Release: 1alt
%else
Release: 1
%endif

%description
%{summary}

%build -a
cat << EOF > README.distro
Add some distro specific notes.
EOF

%files
%doc README.distro
%if %{with alt}
/alt/%{_bindir}/alt-hello
/alt/%{_docdir}/%{name}
%else
%{_bindir}/hello
%{_docdir}/%{name}
%endif
