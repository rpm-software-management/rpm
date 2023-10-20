Name: append
Version: 1.0
Release: 1
BuildArch: noarch
Summary: Testing scriptlet append/prepend
License: GPL

%description
%{summary}

%build
echo BBB >> out

%build -a
echo CCC >> out

%build -p
echo AAA >> out

%build -a
echo DDD >> out

%build -p
echo 000 >> out

%install -a
cp out ${RPM_BUILD_ROOT}/opt/

%install -p
mkdir -p ${RPM_BUILD_ROOT}/opt/

%files
/opt/out
