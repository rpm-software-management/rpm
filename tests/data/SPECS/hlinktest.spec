Summary:          Testing hard link behavior
Name:             hlinktest
Version:          1.0
Release:          1
License:          Testing
Group:            Testing
BuildArch:	  noarch
%description
  
%install
rm -rf %{buildroot}
mkdir -p $RPM_BUILD_ROOT/foo
cat << EOF >> $RPM_BUILD_ROOT/foo/hello
#!/bin/sh
echo %{name}-%{version}
EOF

cd $RPM_BUILD_ROOT/foo
cat hello > copyllo
cat hello > aaaa
ln aaaa zzzz
chmod a+x hello copyllo
for f in foo bar world; do
    ln hello hello-${f}
done

%files
%defattr(-,root,root)
/foo/*
