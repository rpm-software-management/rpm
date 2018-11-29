Name:           capstest
Version:        1.0
Release:        1
Summary:        Testing file capabilities behavior
Group:          Testing
License:        GPL
BuildArch:      noarch

%description
%{summary}

%install
rm -rf $RPM_BUILD_ROOT

mkdir -p $RPM_BUILD_ROOT/a
echo "x" > $RPM_BUILD_ROOT/a/emptyCaps1
echo "x" > $RPM_BUILD_ROOT/a/emptyCaps2
echo "x" > $RPM_BUILD_ROOT/a/noCaps


%files -n %{name}
%attr(0777,root,root) %caps(=) /a/emptyCaps1
%attr(0777,root,root) %caps()  /a/emptyCaps2
%attr(0777,root,root)          /a/noCaps


%changelog
* Mon Nov 5 2018 Pavlina Moravcova Varekova <pmorevco@redhat.com>
- Initial package
