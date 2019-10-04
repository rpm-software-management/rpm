Name:           shebang
Version:        0.1
Release:        1
Summary:        Testing shebang dependency generation
Group:          Testing
License:        GPL
BuildArch:	noarch

%description
%{summary}

%install
mkdir -p %{buildroot}/bin
cat << EOF > %{buildroot}/bin/shebang
#!/bin/blabla
echo shebang
EOF

chmod a+x %{buildroot}/bin/shebang

%files
%defattr(-,root,root,-)
/bin/shebang
