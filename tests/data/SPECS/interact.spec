Name: interact
Version: 1.0
Release: 1
Summary: test
License: Public Domain

# This must abort the build
%prep
read EHLO
echo hello $EHLO

%files
