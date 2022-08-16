# Make sure %%doc files are installed into a hard-coded prefix so that we can
# then compare them against a static list using "rpm -qpl" in
# tests/rpmbuild.at.
%global _prefix /opt

Name:           filemisstest
Version:        1.0
Release:        1
Summary:        Testing missing file behavior
Group:          Testing
License:        GPL
BuildArch:	noarch

%description
%{summary}.


%build
%install

%files
%doc /opt/share/doc/%{name}-%{version}/CREDITS
%doc INSTALL README*
%license LICENSE OTHERLICENSE?
/opt/foo
/opt/bar{a,b}
