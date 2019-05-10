%{!?ver:%define ver 1.0}

Name:           scriptfail
Version:        %{ver}
Release:        1
Summary:        Testing script errore behavior

Group:          Testing
License:        GPL
BuildArch:	noarch

%description
%{summary}

%{lua:
for i, s in ipairs({"pre","preun","pretrans","post","postun","posttrans"}) do
   print('%'..s..' -e\n')
   print('%{!?exit'..s..':%global exit'..s..' 0}\n')
   print('exit %{exit'..s..'}\n\n')
end
}

%files
%defattr(-,root,root,-)
