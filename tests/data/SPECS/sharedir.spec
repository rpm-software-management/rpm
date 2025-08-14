Name: sharedir
Version: 1.0
Release: 1
License: Public domain
BuildArch: noarch
Summary: Testing shared directory behavior

%description
%{summary}

%package a
Summary: %{summary}
Requires: %{name}

%description a
%{summary}

%package b
Summary: %{summary}
Requires: %{name}

%description b
%{summary}

%package c
Summary: %{summary}
Requires: %{name}

%description c
%{summary}

%install
mkdir -p ${RPM_BUILD_ROOT}/opt/sharedir/sub
for x in a b c; do
    echo ${x} > ${RPM_BUILD_ROOT}/opt/sharedir/sub/${x}
done

%files
/opt/sharedir/

%files a
%dir /opt/sharedir/sub/
/opt/sharedir/sub/a

%files b
%dir /opt/sharedir/sub/
/opt/sharedir/sub/b

%files c
%dir /opt/sharedir/sub/
/opt/sharedir/sub/c
