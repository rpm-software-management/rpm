Name:		hello-script
Version:	1.0
Release:	1
Summary:	Testing file conflict behavior

Group:		Testing
License:	GPL
BuildArch:	noarch

%description
%{summary}

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin
cat << EOF > $RPM_BUILD_ROOT/usr/bin/hello
echo "Hello world!"
EOF

%files
%defattr(-,root,root,-)
/usr/bin/hello
