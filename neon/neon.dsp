# Microsoft Developer Studio Project File - Name="neon" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=neon - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "neon.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "neon.mak" CFG="neon - Win32 Debug"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "neon - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "neon - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "neon - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D 
"_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "$(NEON_EXPAT_LITE_WIN32)" /D "NDEBUG" 
/D "WIN32" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Release\neons.lib"

!ELSEIF  "$(CFG)" == "neon - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" 
/D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "$(NEON_EXPAT_LITE_WIN32)" /D 
"_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Debug\neonsd.lib"

!ENDIF

# Begin Target

# Name "neon - Win32 Release"
# Name "neon - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\src\base64.c
# End Source File
# Begin Source File

SOURCE=.\src\dates.c
# End Source File
# Begin Source File

SOURCE=.\src\dav_207.c
# End Source File
# Begin Source File

SOURCE=.\src\dav_basic.c
# End Source File
# Begin Source File

SOURCE=.\src\dav_locks.c
# End Source File
# Begin Source File

SOURCE=.\src\dav_props.c
# End Source File
# Begin Source File

SOURCE=.\src\hip_xml.c
# End Source File
# Begin Source File

SOURCE=.\src\http_auth.c
# End Source File
# Begin Source File

SOURCE=.\src\http_basic.c
# End Source File
# Begin Source File

SOURCE=.\src\http_cookies.c
# End Source File
# Begin Source File

SOURCE=.\src\http_redirect.c
# End Source File
# Begin Source File

SOURCE=.\src\http_request.c
# End Source File
# Begin Source File

SOURCE=.\src\http_utils.c
# End Source File
# Begin Source File

SOURCE=.\src\md5.c
# End Source File
# Begin Source File

SOURCE=.\src\ne_alloc.c
# End Source File
# Begin Source File

SOURCE=.\src\neon_i18n.c
# End Source File
# Begin Source File

SOURCE=.\src\socket.c
# End Source File
# Begin Source File

SOURCE=.\src\sslcerts.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\src\string_utils.c
# End Source File
# Begin Source File

SOURCE=.\src\uri.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Generated Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\config.h
# End Source File
# Begin Source File

SOURCE=.\config.hw

!IF  "$(CFG)" == "neon - Win32 Release"

# Begin Custom Build
InputPath=.\config.hw

".\config.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy .\config.hw .\config.h > nul
	echo Created config.h from config.hw

# End Custom Build

!ELSEIF  "$(CFG)" == "neon - Win32 Debug"

# Begin Custom Build
InputPath=.\config.hw

".\src\config.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy .\config.hw .\src\config.h > nul
	echo Created config.h from config.hw

# End Custom Build

!ENDIF

# End Source File
# End Group
# End Target
# End Project

