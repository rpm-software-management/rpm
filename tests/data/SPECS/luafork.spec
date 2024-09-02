Name: luafork
Version: 1.0
Release: 1
License: Public domain
Summary: Testing Lua fork behavior
BuildArch: noarch

%description
%{summary}

%pre -p <lua>
local pid = posix.fork()
if pid == 0 then
    io.stdout:write("child\n")
    os.exit(0)
elseif pid > 0 then
    posix.wait(pid)
else
    io.stderr:write("fork failed")
end

%files
