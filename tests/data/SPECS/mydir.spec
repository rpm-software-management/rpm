# these *are* different but %%global is widely misused, it only works
# because rpm lets the undefined macros pass for later expansion
%global mydir1 %{_builddir}/%{buildsubdir}
%define mydir2 %{_builddir}/%{buildsubdir}

Name: mydir
Version: 1.0
Release: 1
BuildArch: noarch
Summary: Test builddir oddities
License: GPL

%prep
%setup -T -c

%build
echo mydir1=%{mydir1}
echo mydir2=%{mydir2}

%install

%files
