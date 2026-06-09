Summary:       Test Spec file
Name:          autosetup-test
Version:       1.2.3
Release:       4
License:       GPL

Source:        source-noroot-nodir.tar.gz

%description
Test package for %autosetup -C

%prep
%autosetup -C
[ -e file1 ]
[ -e file2 ]
