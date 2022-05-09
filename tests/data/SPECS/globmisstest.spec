# Make sure %%doc files are installed into a hard-coded prefix so that we can
# then compare them against a static list using "rpm -qpl" in
# tests/rpmbuild.at.
%global _prefix /opt

Name:           globmisstest
Version:        1.0
Release:        1
Summary:        Testing missing file glob behavior
Group:          Testing
License:        GPL
BuildArch:	noarch

%description
%{summary}.


%build
%install

%files
%doc foo*
/opt/foo*
