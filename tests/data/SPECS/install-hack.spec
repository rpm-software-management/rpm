# Mimic what fedora kernel.spec does. Whether this is a legitimate
# thing for a package to or not is another question, but at least we'll
# be aware if the behavior changes.
%global __spec_install_pre %{___build_pre}

Summary: install scriptlet section override hack
Name: install-hack
Version: 1.0
Release: 1
License: GPL
Buildarch: noarch

%description
%{summary}

%install
mkdir -p $RPM_BUILD_ROOT/usr/local/bin

%files
/usr/local/bin/
