Name:		selfconflict
Version:	1.0
Release:	1
Summary:	Testing file conflict behavior within package itself

Group:		Testing
License:	GPL
BuildArch:	noarch

%description
%{summary}

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/opt/mydir/{one,two}
echo "foo" > $RPM_BUILD_ROOT/opt/mydir/one/somefile
echo "bar" > $RPM_BUILD_ROOT/opt/mydir/two/somefile

%files
%defattr(-,root,root,-)
/opt/mydir
