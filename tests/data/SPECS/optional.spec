Summary: optional -- test for the %%optional tag
Name: optional
Version: 1.0
Release: 1
Group: Utilities
License: GPL
BuildArch: noarch
URL: http://openmandriva.org/

%description
Test for files with the %%optional tag

%prep

%build

%install
mkdir -p %{buildroot}/dir-exists \
	%{buildroot}/dirglob-exists-1 \
	%{buildroot}/dirglob-exists-2
touch %{buildroot}/file-exists \
	%{buildroot}/glob-exists-1 \
	%{buildroot}/glob-exists-2
ln -s file-exists %{buildroot}/symlink-exists
ln -s / %{buildroot}/symlink-to-dir-exists

%files
%defattr(-,root,root)
%optional /file-exists
%optional /file-does-not-exist
%optional /glob-exists-*
%optional /glob-does-not-exist-*
%optional /dir-exists
%optional /dir-does-not-exist
%optional /dirglob-exists-*
%optional /symlink-exists
%optional /symlink-does-not-exist
%optional /symlink-to-dir-exists
%optional /symlink-to-dir-does-not-exist
