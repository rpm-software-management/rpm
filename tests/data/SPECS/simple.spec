Name:           simple
Version:        1.0
Release:        1
Summary:        Simple test package
Group:		Testing
License:        GPL
BuildArch:	noarch

%description
%{summary}

%install
mkdir -p $RPM_BUILD_ROOT/opt/bin
cat << EOF > $RPM_BUILD_ROOT/opt/bin/simple
#!/bin/sh
echo yay
EOF

chmod a+x $RPM_BUILD_ROOT/opt/bin/simple

%post
touch /var/lib/simple

%files
%defattr(-,root,root,-)
/opt/bin/simple
