Name: dynamic
Version: 1.0
Release: 1
BuildArch: noarch
%{?!FULLDYNAMIC:
Group: Utilities
License: GPL
Distribution: RPM test suite.
URL: http://rpm.org
Summary: dynamic hello -- hello, world rpm
}

%description
Simple rpm demonstration.

%prep
%setup -q -T -c

%build
echo "Q: Why?\nA: Because we can!" > FAQ

%install
mkdir -p $RPM_BUILD_ROOT/usr/local/bin
echo " " > $RPM_BUILD_ROOT/usr/local/bin/hello

%{?FULLDYNAMIC:
echo "Group: Utilities" >> %{specpartsdir}/mainpkg.specpart
echo "License: GPL" >> %{specpartsdir}/mainpkg.specpart
echo "Distribution: RPM test suite." >> %{specpartsdir}/mainpkg.specpart
echo "URL: http://rpm.org" >> %{specpartsdir}/mainpkg.specpart
echo "Summary: dynamic hello -- hello, world rpm" >> %{specpartsdir}/mainpkg.specpart
}
%{?DOUBLESUMMARY:
echo "Summary: dynamic hello -- hello, world again" >> %{specpartsdir}/mainpkg.specpart
}
%{?WRONGTAG:
echo "LicenseToKill: True" >> %{specpartsdir}/mainpkg.specpart
}


echo "%package docs" >> %{specpartsdir}/docs.specpart
%{?!FAIL:echo "Summary: Documentation for dynamic spec" >> %{specpartsdir}/docs.specpart}
echo "BuildArch: noarch" >> %{specpartsdir}/docs.specpart
%{?FORBIDDENTAG:echo "BuildRequires: python3" >> %{specpartsdir}/docs.specpart}
%{?FORBIDDENSECTION:echo "%check" >> %{specpartsdir}/docs.specpart}
echo "%description docs" >> %{specpartsdir}/docs.specpart
echo "Test for dynamically generated spec files" >> %{specpartsdir}/docs.specpart
echo "%files docs" >> $RPM_SPECPARTS_DIR/docs.specpart
echo "%doc	FAQ" >> $RPM_SPECPARTS_DIR/docs.specpart

%files
%defattr(-,root,root)
%attr(0751,root,root)	/usr/local/bin/hello

%changelog
* Mon Oct 24 2022 Florian Festi <ffesti@redhat.com>
- create.
