Name:		filetriggers-prefix
Version:	1.0
Release:	1
Summary:	Testing multiple file trigger prefixes
BuildArch:	noarch
License:	GPL

%description
%{summary}

%filetriggerin -- /foo /usr/bin /etc
echo "filetriggerin:"
cat | xargs dirname | uniq
echo

%filetriggerun -- /etc /foo /usr/bin
echo "filetriggerun:"
cat | xargs dirname | uniq
echo

%filetriggerpostun -- /foo /usr/bin /etc
echo "filetriggerpostun:"
cat | xargs dirname | uniq
echo

%transfiletriggerin -- /foo /etc /usr/bin
echo "transfiletriggerin:"
cat | xargs dirname | uniq
echo

%transfiletriggerun -- /foo /usr/bin /etc
echo "transfiletriggerun:"
cat | xargs dirname | uniq
echo

%transfiletriggerpostun -- /usr/bin /etc
echo "transfiletriggerpostun:"
echo /usr/bin
echo

%transfiletriggerpostun -- /opt /foo
echo "transfiletriggerpostun:"
echo /foo
echo

%files
