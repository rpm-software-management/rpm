%{!?ver:%define ver 1.0}
%{!?trigpath:%define trigpath /usr/bin}

Name:           ftrigfail
Version:        %{ver}
Release:        1
Summary:        Testing file trigger error behavior

Group:          Testing
License:        GPL
BuildArch:	noarch

%description
%{summary}

%{lua:
t = { "filetriggerin", "filetriggerun", "filetriggerpostun",
      "transfiletriggerin", "transfiletriggerun", "transfiletriggerpostun" }
for i, s in ipairs(t) do
   print(string.format('%%%s -e -- %s\n', s, macros.trigpath))
   print(string.format('echo %s\n', s:upper()))
   print(string.format('cat\n'))
   print(string.format('%%{?fail%s:exit 1}\n\n', s))
end
}

%files
%defattr(-,root,root,-)
