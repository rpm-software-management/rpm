Name:		parallel
Version:	%{ver}
Release:	1%{?dist}
Summary:	Testing parallel install behavior
BuildArch:	noarch
License:	GPL

%description
%{summary}

%package trigger
Summary: %{summary}

%description trigger
%{summary}

%transfiletriggerin trigger -- /opt/parallel
echo %transfiletriggerin: $*
cat | wc -l

%transfiletriggerun trigger -- /opt/parallel
echo %transfiletriggerun: $*
cat | wc -l

%transfiletriggerpostun trigger -- /opt/parallel
echo %transfiletriggerpostun: $*
cat | wc -l

%filetriggerin trigger -- /opt/parallel
echo %filetriggerin: $*
cat | wc -l

%filetriggerpostun trigger -- /opt/parallel
echo %filetriggerpostun: $*
cat | wc -l


%install
for d in mydir yourdir; do
    datadir=%{buildroot}/opt/parallel/%{version}/${d}
    mkdir -p ${datadir}
    for f in 1 2 3 4; do
        echo ${f} > ${datadir}/${f}
    done
done

%files
/opt/%{name}

%files trigger
