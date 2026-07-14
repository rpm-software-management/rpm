Name:           filetrigger-priority
Version:        1.0
Release:        1
Summary:        Testing file trigger priority ordering
License:        GPL
BuildArch:      noarch

%description
%{summary}

%package data
Summary:        Data for testing file trigger priority ordering

%description data
%{summary}

%install
mkdir -p %{buildroot}/opt/filetrigger-priority
touch %{buildroot}/opt/filetrigger-priority/data

%transfiletriggerin -n filetrigger-priority -P 100 -- /opt/filetrigger-priority
echo "priority 100"

%transfiletriggerin -n filetrigger-priority -P 200 -- /opt/filetrigger-priority
echo "priority 200"

%files

%files data
/opt/filetrigger-priority/data
