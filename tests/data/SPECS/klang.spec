Name: klang
Version: 1.0
Release: 1
BuildArch: noarch
License: GPL
Summary: Klang frobnizer

%description
%{summary}

%package common
Summary: %{SUMMARY} common

%description common
%{summary}

%package client
Summary: %{SUMMARY} client

%description client
%{summary}

%package server
Summary: %{SUMMARY} server

%description server
%{summary}

%install
mkdir -p ${RPM_BUILD_ROOT}/var/lib/klangd
mkdir -p ${RPM_BUILD_ROOT}/usr/bin/
mkdir -p ${RPM_BUILD_ROOT}/etc
mkdir -p ${RPM_BUILD_ROOT}/%{_sysusersdir}

echo "aaaa" > ${RPM_BUILD_ROOT}/usr/bin/klang
echo "bbbb" > ${RPM_BUILD_ROOT}/usr/bin/klangd
echo "xxxx" > ${RPM_BUILD_ROOT}/etc/klang.cfg

cat << EOF > ${RPM_BUILD_ROOT}/%{_sysusersdir}/klang.conf
g klang -
EOF

cat << EOF > ${RPM_BUILD_ROOT}/%{_sysusersdir}/klangd.conf
u klangd - "Klang server" /var/lib/klangd /sbin/nologin
EOF
cat << EOF > ${RPM_BUILD_ROOT}/%{_sysusersdir}/plong.conf
u plong - "Plong fu" /var/lib/plong /sbin/nologin
g klong -
EOF

%files common
%{_sysusersdir}/klang.conf
%attr(-,-,klang) /etc/klang.cfg

%files client
%attr(-,-,klang) /usr/bin/klang

%files server
%{_sysusersdir}/klangd.conf
%{_sysusersdir}/plong.conf
%attr(-,klangd,klangd) /var/lib/klangd
%attr(-,plong,klong) /var/lib/klangd
/usr/bin/klangd
