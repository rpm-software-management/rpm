Name:           verifyscript
Version:        1.0
Release:        1
Summary:        Testing verifyscript behavior

Group:          Testing
License:        GPL
BuildArch:	noarch

%description
%{summary}

%verifyscript -p <lua>
if not posix.access("/var/checkme", "f") then
   error("bad")
end

%files
%defattr(-,root,root)
