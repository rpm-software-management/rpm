# Microsoft Developer Studio Generated NMAKE File, Based on neon.dsp
!IF "$(CFG)" == ""
CFG=neon - Win32 Release
!MESSAGE No configuration specified. Defaulting to neon - Win32 Release
!ENDIF 

!IF "$(CFG)" != "neon - Win32 Release" && "$(CFG)" != "neon - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
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
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(EXPAT_SRC)" == ""
!MESSAGE  
!MESSAGE Need expat-lite source directory
!MESSAGE  
!MESSAGE Call with
!MESSAGE nmake /f neon.mak EXPAT_SRC=D:\apache_1.3.20\src\lib\expat-lite 
!MESSAGE  
!ERROR 
!ENDIF 


!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "neon - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : ".\config.h" "$(OUTDIR)\neons.lib"


CLEAN :
	-@erase "$(INTDIR)\base64.obj"
	-@erase "$(INTDIR)\ne_207.obj"
	-@erase "$(INTDIR)\ne_alloc.obj"
	-@erase "$(INTDIR)\ne_auth.obj"
	-@erase "$(INTDIR)\ne_basic.obj"
	-@erase "$(INTDIR)\ne_cookies.obj"
	-@erase "$(INTDIR)\ne_dates.obj"
	-@erase "$(INTDIR)\ne_i18n.obj"
	-@erase "$(INTDIR)\ne_locks.obj"
	-@erase "$(INTDIR)\ne_md5.obj"
	-@erase "$(INTDIR)\ne_props.obj"
	-@erase "$(INTDIR)\ne_redirect.obj"
	-@erase "$(INTDIR)\ne_request.obj"
	-@erase "$(INTDIR)\ne_session.obj"
	-@erase "$(INTDIR)\ne_socket.obj"
	-@erase "$(INTDIR)\ne_string.obj"
	-@erase "$(INTDIR)\ne_uri.obj"
	-@erase "$(INTDIR)\ne_utils.obj"
	-@erase "$(INTDIR)\ne_xml.obj"
	-@erase "$(OUTDIR)\neons.lib"
	-@erase ".\config.h"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/c /nologo /MD /W3 /GX /O2 /I "$(EXPAT_SRC)" /D "NDEBUG" /D "HAVE_CONFIG_H" /Fo"$(INTDIR)\\" 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\neon.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\neons.lib" 
LIB32_OBJS= \
	"$(INTDIR)\base64.obj" \
	"$(INTDIR)\ne_xml.obj" \
	"$(INTDIR)\ne_alloc.obj" \
	"$(INTDIR)\ne_auth.obj" \
	"$(INTDIR)\ne_basic.obj" \
	"$(INTDIR)\ne_cookies.obj" \
	"$(INTDIR)\ne_dates.obj" \
	"$(INTDIR)\ne_i18n.obj" \
	"$(INTDIR)\ne_locks.obj" \
	"$(INTDIR)\ne_md5.obj" \
	"$(INTDIR)\ne_props.obj" \
	"$(INTDIR)\ne_redirect.obj" \
	"$(INTDIR)\ne_request.obj" \
	"$(INTDIR)\ne_session.obj" \
	"$(INTDIR)\ne_socket.obj" \
	"$(INTDIR)\ne_string.obj" \
	"$(INTDIR)\ne_uri.obj" \
	"$(INTDIR)\ne_utils.obj" \
	"$(INTDIR)\ne_207.obj"

"$(OUTDIR)\neons.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "neon - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\neonsd.lib"


CLEAN :
	-@erase "$(INTDIR)\base64.obj"
	-@erase "$(INTDIR)\ne_207.obj"
	-@erase "$(INTDIR)\ne_alloc.obj"
	-@erase "$(INTDIR)\ne_auth.obj"
	-@erase "$(INTDIR)\ne_basic.obj"
	-@erase "$(INTDIR)\ne_cookies.obj"
	-@erase "$(INTDIR)\ne_dates.obj"
	-@erase "$(INTDIR)\ne_i18n.obj"
	-@erase "$(INTDIR)\ne_locks.obj"
	-@erase "$(INTDIR)\ne_md5.obj"
	-@erase "$(INTDIR)\ne_props.obj"
	-@erase "$(INTDIR)\ne_redirect.obj"
	-@erase "$(INTDIR)\ne_request.obj"
	-@erase "$(INTDIR)\ne_session.obj"
	-@erase "$(INTDIR)\ne_socket.obj"
	-@erase "$(INTDIR)\ne_string.obj"
	-@erase "$(INTDIR)\ne_uri.obj"
	-@erase "$(INTDIR)\ne_utils.obj"
	-@erase "$(INTDIR)\ne_xml.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\neonsd.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/c /nologo /MDd /W3 /Gm /GX /Zi /Od /I "$(EXPAT_SRC)" /D "HAVE_CONFIG_H" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\neon.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\neonsd.lib" 
LIB32_OBJS= \
	"$(INTDIR)\base64.obj" \
	"$(INTDIR)\ne_xml.obj" \
	"$(INTDIR)\ne_alloc.obj" \
	"$(INTDIR)\ne_auth.obj" \
	"$(INTDIR)\ne_basic.obj" \
	"$(INTDIR)\ne_cookies.obj" \
	"$(INTDIR)\ne_dates.obj" \
	"$(INTDIR)\ne_i18n.obj" \
	"$(INTDIR)\ne_locks.obj" \
	"$(INTDIR)\ne_md5.obj" \
	"$(INTDIR)\ne_props.obj" \
	"$(INTDIR)\ne_redirect.obj" \
	"$(INTDIR)\ne_request.obj" \
	"$(INTDIR)\ne_session.obj" \
	"$(INTDIR)\ne_socket.obj" \
	"$(INTDIR)\ne_string.obj" \
	"$(INTDIR)\ne_uri.obj" \
	"$(INTDIR)\ne_utils.obj" \
	"$(INTDIR)\ne_207.obj"

"$(OUTDIR)\neonsd.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 


#!IF "$(NO_EXTERNAL_DEPS)" != "1"
#!IF EXISTS("neon.dep")
#!INCLUDE "neon.dep"
#!ELSE 
#!MESSAGE Warning: cannot find "neon.dep"
#!ENDIF 
#!ENDIF 


!IF "$(CFG)" == "neon - Win32 Release" || "$(CFG)" == "neon - Win32 Debug"
SOURCE=.\src\base64.c

"$(INTDIR)\base64.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ne_207.c

"$(INTDIR)\ne_207.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ne_alloc.c

"$(INTDIR)\ne_alloc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ne_auth.c

"$(INTDIR)\ne_auth.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ne_basic.c

"$(INTDIR)\ne_basic.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ne_cookies.c

"$(INTDIR)\ne_cookies.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ne_dates.c

"$(INTDIR)\ne_dates.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ne_i18n.c

"$(INTDIR)\ne_i18n.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ne_locks.c

"$(INTDIR)\ne_locks.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ne_md5.c

"$(INTDIR)\ne_md5.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ne_props.c

"$(INTDIR)\ne_props.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ne_redirect.c

"$(INTDIR)\ne_redirect.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ne_request.c

"$(INTDIR)\ne_request.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ne_session.c

"$(INTDIR)\ne_session.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ne_socket.c

"$(INTDIR)\ne_socket.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ne_string.c

"$(INTDIR)\ne_string.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ne_uri.c

"$(INTDIR)\ne_uri.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ne_utils.c

"$(INTDIR)\ne_utils.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\ne_xml.c

"$(INTDIR)\ne_xml.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\src\sslcerts.c
SOURCE=.\config.hw

!IF  "$(CFG)" == "neon - Win32 Release"

InputPath=.\config.hw

".\config.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	copy .\config.hw .\src\config.h > nul 
	echo Created config.h from config.hw
<< 
	

!ELSEIF  "$(CFG)" == "neon - Win32 Debug"

InputPath=.\config.hw

".\src\config.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	copy .\config.hw .\src\config.h > nul 
	echo Created config.h from config.hw
<< 
	

!ENDIF 


!ENDIF 

