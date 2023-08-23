Name: badftrigger
Version: 1.0
Release: 1
Summary: Testing error reporting
License: GPL
BuildArch: noarch

%description
%{summary}

%transfiletriggerin -- /something %{nosuchmacro}
echo bad

%files
