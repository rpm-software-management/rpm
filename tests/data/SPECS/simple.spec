%bcond setup 0

Name:           simple
Version:        1.0
Release:        1
Summary:        Simple test package
Group:		Testing
License:        GPL
BuildArch:	noarch
Source:		source-noroot.tar.gz

%description
%{summary}

%if %{with setup}
%prep
%setup -C
%endif

%build
cat << EOF > simple
#!/bin/sh
echo yay
EOF
chmod a+x simple

%install
mkdir -p $RPM_BUILD_ROOT/opt/bin
cp simple $RPM_BUILD_ROOT/opt/bin/

%post
touch /var/lib/simple

%files
%defattr(-,root,root,-)
/opt/bin/simple
