%bcond_with unpackaged_dirs
%bcond_with unpackaged_files
%bcond_with unpackaged_excludes

Summary:          Testing hard link behavior
Name:             hlinktest
Version:          1.0
Release:          1
License:          Testing
Group:            Testing
BuildArch:	  noarch
Provides: /bin/sh
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

%if %{with unpackaged_dirs}
mkdir -p $RPM_BUILD_ROOT/zoo/
%endif

%if %{with unpackaged_files}
touch $RPM_BUILD_ROOT/toot
%endif

%if %{with unpackaged_excludes}
touch $RPM_BUILD_ROOT/teet
%endif

%files
%defattr(-,root,root)
/foo/*
%if %{with unpackaged_excludes}
%exclude /teet
%endif
