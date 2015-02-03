Name:           filetriggers
Version:        1.0
Release:        1
Summary:        Testing file triggers

Group:          testing
License:        GPL
BuildArch:      noarch


%description
%{summary}


%filetriggerin --  /usr/bin
echo "filetriggerin(/usr/bin*):"
cat
echo

%filetriggerun --  /usr/bin
echo "filetriggerun(/usr/bin*):"
cat
echo

%filetriggerpostun --  /usr/bin
echo "filetriggerpostun(/usr/bin*):"
cat
echo

%transfiletriggerin -- /usr/bin
echo "transfiletriggerin(/usr/bin*):"
cat
echo

%transfiletriggerun -- /usr/bin
echo "transfiletriggerun(/usr/bin*):"
cat
echo

%transfiletriggerpostun -- /usr/bin
echo "transfiletriggerpostun(/usr/bin*):"
cat
echo

%filetriggerin -p <lua> -- /usr/bin
print("filetriggerin(/usr/bin*)<lua>:")
a = rpm.next_file()
while a do
    print(a)
    a = rpm.next_file()
end
print("")
io.flush()

%filetriggerin -- /foo
echo "filetriggerin(/foo*):"
cat
echo

%filetriggerun -- /foo
echo "filetriggerun(/foo*):"
cat
echo

%filetriggerpostun -- /foo
echo "filetriggerpostun(/foo*):"
cat
echo

%transfiletriggerin -- /foo
echo "transfiletriggerin(/foo*):"
cat
echo

%transfiletriggerun -- /foo
echo "transfiletriggerun(/foo*):"
cat
echo

%transfiletriggerpostun -- /foo
echo "transfiletriggerpostun(/foo*):"
cat
echo

%filetriggerin -p <lua> -- /foo
print("filetriggerin(/foo*)<lua>:")
a = rpm.next_file()
while a do
    print(a)
    a = rpm.next_file()
end
print("")
io.flush()

%files
