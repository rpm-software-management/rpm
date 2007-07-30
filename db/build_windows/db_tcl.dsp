# Microsoft Developer Studio Project File - Name="db_tcl" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=db_tcl - Win32 Debug x86
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "db_tcl.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "db_tcl.mak" CFG="db_tcl - Win32 Debug x86"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "db_tcl - Win32 Release x86" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "db_tcl - Win32 Debug x86" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "db_tcl - Win32 ASCII Debug x86" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "db_tcl - Win32 ASCII Release x86" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "db_tcl - x64 Debug AMD64" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "db_tcl - x64 Release AMD64" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "db_tcl - x64 Debug IA64" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "db_tcl - x64 Release IA64" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "db_tcl - Win32 Debug AMD64" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "db_tcl - Win32 Release AMD64" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "db_tcl - Win32 Debug IA64" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "db_tcl - Win32 Release IA64" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "db_tcl - Win32 Release x86"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release/db_tcl"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release/db_tcl"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /Ob2 /I "." /I ".." /D "UNICODE" /D "_UNICODE" /D "DB_CREATE_DLL" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "DB_TCL_SUPPORT" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 libdb46.lib tcl84.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 libdb46.lib tcl84.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /machine:I386 /out:"Release/libdb_tcl46.dll" /libpath:"$(OUTDIR)"

!ELSEIF  "$(CFG)" == "db_tcl - Win32 Debug x86"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug/db_tcl"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug/db_tcl"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FD /c
# ADD CPP /nologo /MDd /W3 /GX /Z7 /Od /I "." /I ".." /D "UNICODE" /D "_UNICODE" /D "DB_CREATE_DLL" /D "DIAGNOSTIC" /D "CONFIG_TEST" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "DB_TCL_SUPPORT" /FD /c
# SUBTRACT CPP /Fr
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 libdb46d.lib tcl84g.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 libdb46d.lib tcl84g.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /export:__db_assert /pdb:none /debug /machine:I386 /out:"Debug/libdb_tcl46d.dll" /fixed:no /libpath:"$(OUTDIR)"

!ELSEIF  "$(CFG)" == "db_tcl - Win32 ASCII Debug x86"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug_ASCII"
# PROP BASE Intermediate_Dir "Debug_ASCII/db_tcl"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_ASCII"
# PROP Intermediate_Dir "Debug_ASCII/db_tcl"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /GX /Z7 /Od /I "." /I ".." /D "DB_CREATE_DLL" /D "DIAGNOSTIC" /D "CONFIG_TEST" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "DB_TCL_SUPPORT" /FD /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /MDd /W3 /GX /Z7 /Od /I "." /I ".." /D "DB_CREATE_DLL" /D "DIAGNOSTIC" /D "CONFIG_TEST" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "DB_TCL_SUPPORT" /FD /c
# SUBTRACT CPP /Fr
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 libdb46d.lib tcl84g.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /pdb:none /debug /machine:I386 /out:"Debug_ASCII/libdb_tcl46d.dll" /fixed:no
# ADD LINK32 libdb46d.lib tcl84g.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /export:__db_assert /pdb:none /debug /machine:I386 /out:"Debug_ASCII/libdb_tcl46d.dll" /fixed:no /libpath:"$(OUTDIR)"

!ELSEIF  "$(CFG)" == "db_tcl - Win32 ASCII Release x86"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release_ASCII"
# PROP BASE Intermediate_Dir "Release_ASCII/db_tcl"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_ASCII"
# PROP Intermediate_Dir "Release_ASCII/db_tcl"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /Ob2 /I "." /I ".." /D "DB_CREATE_DLL" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "DB_TCL_SUPPORT" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /Ob2 /I "." /I ".." /D "DB_CREATE_DLL" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "DB_TCL_SUPPORT" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 libdb46.lib tcl84.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /machine:I386 /out:"Release_ASCII/libdb_tcl46.dll"
# ADD LINK32 libdb46.lib tcl84.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /machine:I386 /out:"Release_ASCII/libdb_tcl46.dll" /libpath:"$(OUTDIR)"

!ELSEIF  "$(CFG)" == "db_tcl - x64 Debug AMD64"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug_AMD64"
# PROP BASE Intermediate_Dir "Debug_AMD64/db_tcl"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_AMD64"
# PROP Intermediate_Dir "Debug_AMD64/db_tcl"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /EHsc /Z7 /Od /I "." /I ".." /D "UNICODE" /D "_UNICODE" /D "DB_CREATE_DLL" /D "DIAGNOSTIC" /D "CONFIG_TEST" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "DB_TCL_SUPPORT" /FD /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /MDd /W3 /EHsc /Z7 /Od /I "." /I ".." /D "UNICODE" /D "_UNICODE" /D "DB_CREATE_DLL" /D "DIAGNOSTIC" /D "CONFIG_TEST" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "DB_TCL_SUPPORT" /FD /Wp64 /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 libdb46d.lib tcl84g.lib bufferoverflowU.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /debug /machine:AMD64 /out:"Debug_AMD64/libdb_tcl46d.dll" /fixed:no
# ADD LINK32 libdb46d.lib tcl84g.lib bufferoverflowU.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /export:__db_assert /debug /machine:AMD64 /out:"Debug_AMD64/libdb_tcl46d.dll" /fixed:no /libpath:"$(OUTDIR)"

!ELSEIF  "$(CFG)" == "db_tcl - x64 Release AMD64"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release_AMD64"
# PROP BASE Intermediate_Dir "Release_AMD64/db_tcl"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_AMD64"
# PROP Intermediate_Dir "Release_AMD64/db_tcl"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /EHsc /O2 /Ob2 /I "." /I ".." /D "UNICODE" /D "_UNICODE" /D "DB_CREATE_DLL" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "DB_TCL_SUPPORT" /Wp64 /FD /c
# ADD CPP /nologo /MD /W3 /EHsc /O2 /Ob2 /I "." /I ".." /D "UNICODE" /D "_UNICODE" /D "DB_CREATE_DLL" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "DB_TCL_SUPPORT" /Wp64 /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 libdb46.lib tcl84.lib bufferoverflowU.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /machine:AMD64 /out:"Release_AMD64/libdb_tcl46.dll"
# ADD LINK32 libdb46.lib tcl84.lib bufferoverflowU.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /machine:AMD64 /out:"Release_AMD64/libdb_tcl46.dll" /libpath:"$(OUTDIR)"

!ELSEIF  "$(CFG)" == "db_tcl - x64 Debug IA64"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug_IA64"
# PROP BASE Intermediate_Dir "Debug_IA64/db_tcl"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_IA64"
# PROP Intermediate_Dir "Debug_IA64/db_tcl"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /EHsc /Z7 /Od /I "." /I ".." /D "UNICODE" /D "_UNICODE" /D "DB_CREATE_DLL" /D "DIAGNOSTIC" /D "CONFIG_TEST" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "DB_TCL_SUPPORT" /Wp64 /FD /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /MDd /W3 /EHsc /Z7 /Od /I "." /I ".." /D "UNICODE" /D "_UNICODE" /D "DB_CREATE_DLL" /D "DIAGNOSTIC" /D "CONFIG_TEST" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "DB_TCL_SUPPORT" /Wp64 /FD /c
# SUBTRACT CPP /Fr
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 libdb46d.lib tcl84g.lib bufferoverflowU.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /debug /machine:IA64 /out:"Debug_IA64/libdb_tcl46d.dll" /fixed:no
# ADD LINK32 libdb46d.lib tcl84g.lib bufferoverflowU.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /export:__db_assert /debug /machine:IA64 /out:"Debug_IA64/libdb_tcl46d.dll" /fixed:no /libpath:"$(OUTDIR)"

!ELSEIF  "$(CFG)" == "db_tcl - x64 Release IA64"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release_IA64"
# PROP BASE Intermediate_Dir "Release_IA64/db_tcl"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_IA64"
# PROP Intermediate_Dir "Release_IA64/db_tcl"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /EHsc /O2 /Ob2 /I "." /I ".." /D "UNICODE" /D "_UNICODE" /D "DB_CREATE_DLL" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "DB_TCL_SUPPORT" /Wp64 /FD /c
# ADD CPP /nologo /MD /W3 /EHsc /O2 /Ob2 /I "." /I ".." /D "UNICODE" /D "_UNICODE" /D "DB_CREATE_DLL" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "DB_TCL_SUPPORT" /Wp64 /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 libdb46.lib tcl84.lib bufferoverflowU.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /machine:IA64 /out:"Release_IA64/libdb_tcl46.dll"
# ADD LINK32 libdb46.lib tcl84.lib bufferoverflowU.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /machine:IA64 /out:"Release_IA64/libdb_tcl46.dll" /libpath:"$(OUTDIR)"

!ELSEIF  "$(CFG)" == "db_tcl - Win32 Debug AMD64"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug_AMD64"
# PROP BASE Intermediate_Dir "Debug_AMD64/db_tcl"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_AMD64"
# PROP Intermediate_Dir "Debug_AMD64/db_tcl"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /EHsc /Z7 /Od /I "." /I ".." /D "UNICODE" /D "_UNICODE" /D "DB_CREATE_DLL" /D "DIAGNOSTIC" /D "CONFIG_TEST" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "DB_TCL_SUPPORT" /FD /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /MDd /W3 /EHsc /Z7 /Od /I "." /I ".." /D "UNICODE" /D "_UNICODE" /D "DB_CREATE_DLL" /D "DIAGNOSTIC" /D "CONFIG_TEST" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "DB_TCL_SUPPORT" /FD /Wp64 /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 libdb46d.lib tcl84g.lib bufferoverflowU.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /debug /machine:AMD64 /out:"Debug_AMD64/libdb_tcl46d.dll" /fixed:no
# ADD LINK32 libdb46d.lib tcl84g.lib bufferoverflowU.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /export:__db_assert /debug /machine:AMD64 /out:"Debug_AMD64/libdb_tcl46d.dll" /fixed:no /libpath:"$(OUTDIR)"

!ELSEIF  "$(CFG)" == "db_tcl - Win32 Release AMD64"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release_AMD64"
# PROP BASE Intermediate_Dir "Release_AMD64/db_tcl"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_AMD64"
# PROP Intermediate_Dir "Release_AMD64/db_tcl"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /EHsc /O2 /Ob2 /I "." /I ".." /D "UNICODE" /D "_UNICODE" /D "DB_CREATE_DLL" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "DB_TCL_SUPPORT" /Wp64 /FD /c
# ADD CPP /nologo /MD /W3 /EHsc /O2 /Ob2 /I "." /I ".." /D "UNICODE" /D "_UNICODE" /D "DB_CREATE_DLL" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "DB_TCL_SUPPORT" /Wp64 /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 libdb46.lib tcl84.lib bufferoverflowU.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /machine:AMD64 /out:"Release_AMD64/libdb_tcl46.dll"
# ADD LINK32 libdb46.lib tcl84.lib bufferoverflowU.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /machine:AMD64 /out:"Release_AMD64/libdb_tcl46.dll" /libpath:"$(OUTDIR)"

!ELSEIF  "$(CFG)" == "db_tcl - Win32 Debug IA64"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug_IA64"
# PROP BASE Intermediate_Dir "Debug_IA64/db_tcl"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_IA64"
# PROP Intermediate_Dir "Debug_IA64/db_tcl"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /EHsc /Z7 /Od /I "." /I ".." /D "UNICODE" /D "_UNICODE" /D "DB_CREATE_DLL" /D "DIAGNOSTIC" /D "CONFIG_TEST" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "DB_TCL_SUPPORT" /Wp64 /FD /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /MDd /W3 /EHsc /Z7 /Od /I "." /I ".." /D "UNICODE" /D "_UNICODE" /D "DB_CREATE_DLL" /D "DIAGNOSTIC" /D "CONFIG_TEST" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "DB_TCL_SUPPORT" /Wp64 /FD /c
# SUBTRACT CPP /Fr
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 libdb46d.lib tcl84g.lib bufferoverflowU.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /debug /machine:IA64 /out:"Debug_IA64/libdb_tcl46d.dll" /fixed:no
# ADD LINK32 libdb46d.lib tcl84g.lib bufferoverflowU.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /export:__db_assert /debug /machine:IA64 /out:"Debug_IA64/libdb_tcl46d.dll" /fixed:no /libpath:"$(OUTDIR)"

!ELSEIF  "$(CFG)" == "db_tcl - Win32 Release IA64"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release_IA64"
# PROP BASE Intermediate_Dir "Release_IA64/db_tcl"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_IA64"
# PROP Intermediate_Dir "Release_IA64/db_tcl"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /EHsc /O2 /Ob2 /I "." /I ".." /D "UNICODE" /D "_UNICODE" /D "DB_CREATE_DLL" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "DB_TCL_SUPPORT" /Wp64 /FD /c
# ADD CPP /nologo /MD /W3 /EHsc /O2 /Ob2 /I "." /I ".." /D "UNICODE" /D "_UNICODE" /D "DB_CREATE_DLL" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "DB_TCL_SUPPORT" /Wp64 /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 libdb46.lib tcl84.lib bufferoverflowU.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /machine:IA64 /out:"Release_IA64/libdb_tcl46.dll"
# ADD LINK32 libdb46.lib tcl84.lib bufferoverflowU.lib kernel32.lib user32.lib advapi32.lib shell32.lib /nologo /base:"0x13000000" /subsystem:windows /dll /machine:IA64 /out:"Release_IA64/libdb_tcl46.dll" /libpath:"$(OUTDIR)"

!ENDIF 

# Begin Target

# Name "db_tcl - Win32 Release x86"
# Name "db_tcl - Win32 Debug x86"
# Name "db_tcl - Win32 ASCII Debug x86"
# Name "db_tcl - Win32 ASCII Release x86"
# Name "db_tcl - x64 Debug AMD64"
# Name "db_tcl - x64 Release AMD64"
# Name "db_tcl - x64 Debug IA64"
# Name "db_tcl - x64 Release IA64"
# Name "db_tcl - Win32 Debug AMD64"
# Name "db_tcl - Win32 Release AMD64"
# Name "db_tcl - Win32 Debug IA64"
# Name "db_tcl - Win32 Release IA64"
# Begin Source File

SOURCE=.\libdb_tcl.def
# End Source File
# Begin Source File

SOURCE=..\os\os_abort.c
# End Source File
# Begin Source File

SOURCE=..\tcl\tcl_compat.c
# End Source File
# Begin Source File

SOURCE=..\tcl\tcl_db.c
# End Source File
# Begin Source File

SOURCE=..\tcl\tcl_db_pkg.c
# End Source File
# Begin Source File

SOURCE=..\tcl\tcl_dbcursor.c
# End Source File
# Begin Source File

SOURCE=..\tcl\tcl_env.c
# End Source File
# Begin Source File

SOURCE=..\tcl\tcl_internal.c
# End Source File
# Begin Source File

SOURCE=..\tcl\tcl_lock.c
# End Source File
# Begin Source File

SOURCE=..\tcl\tcl_log.c
# End Source File
# Begin Source File

SOURCE=..\tcl\tcl_mp.c
# End Source File
# Begin Source File

SOURCE=..\tcl\tcl_rep.c
# End Source File
# Begin Source File

SOURCE=..\tcl\tcl_seq.c
# End Source File
# Begin Source File

SOURCE=..\tcl\tcl_txn.c
# End Source File
# Begin Source File

SOURCE=..\tcl\tcl_util.c
# End Source File

# End Target
# End Project
