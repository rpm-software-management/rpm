Name:    test
Version: 1
Release: 1
Summary: test SOURCE_DATE_EPOCH
License: GPLv2
BuildArch: noarch

%global source_date_epoch_from_changelog 0
%global clamp_mtime_to_source_date_epoch 1
%global use_source_date_epoch_as_buildtime 1

%description

%build
echo "this is a test" > 0.txt

%install
%{__install} -m 644 -D 0.txt %{buildroot}/0.txt

%files
/0.txt
