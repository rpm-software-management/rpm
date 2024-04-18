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

%build -p
touch pre1

%build -p
touch pre2

%build -a
test -f pre1 || exit 1
test -f pre2 || exit 1
cat << EOF > README.distro
Add some distro specific notes.
EOF

%install -p
mkdir -p ${RPM_BUILD_ROOT}/%{_sysconfdir}/hello.p

%install -a
test -d ${RPM_BUILD_ROOT}/%{_sysconfdir}/hello.p || exit 1
mkdir -p ${RPM_BUILD_ROOT}/%{_sysconfdir}/hello.d

%install -a
test -d ${RPM_BUILD_ROOT}/%{_sysconfdir}/hello.d || exit 1
mkdir -p ${RPM_BUILD_ROOT}/%{_sysconfdir}/hello.a

%files
%doc README.distro
%{_sysconfdir}/hello.a/
%{_sysconfdir}/hello.d/
%{_sysconfdir}/hello.p/
%if %{with alt}
/alt/%{_bindir}/alt-hello
/alt/%{_docdir}/%{name}
%else
%{_bindir}/hello
%{_docdir}/%{name}
%endif
