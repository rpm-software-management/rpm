#**** neon Win32 -*- Makefile -*- ********************************************
#
# Define DEBUG_BUILD to create a debug version of the library.

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE
NULL=nul
!ENDIF

########
# Support for Expat integration
#
# If EXPAT_SRC or EXPAT_INC are set, then assume compiling against a
# pre-built binary Expat 1.95.X.  You can use either EXPAT_SRC 
# to specify the top-level Expat directory, or EXPAT_INC to directly
# specify the Expat include directory.  (If both are set, EXPAT_SRC
# is ignored).
#
# If EXPAT_SRC and EXPAT_INC are not set, then the user can
# still set EXPAT_FLAGS to specify very specific compile behavior.
#
# If none of EXPAT_SRC, EXPAT_INC and EXPAT_FLAGS are set, disable
# WebDAV support.

!IF "$(EXPAT_INC)" == ""
!IF "$(EXPAT_SRC)" != ""
EXPAT_INC = $(EXPAT_SRC)\Source\Lib
!ENDIF
!ENDIF

BUILD_EXPAT = 1
!IF "$(EXPAT_INC)" == ""
!IFNDEF EXPAT_FLAGS
EXPAT_FLAGS = /D NEON_NODAV
BUILD_EXPAT =
!ENDIF
!ELSE
EXPAT_FLAGS = /I "$(EXPAT_INC)" /D HAVE_EXPAT /D HAVE_EXPAT_H
!ENDIF


########
# Support for OpenSSL integration
!IF "$(OPENSSL_SRC)" == ""
OPENSSL_FLAGS =
!ELSE
OPENSSL_FLAGS = /I "$(OPENSSL_SRC)\inc32" /D NEON_SSL
!ENDIF

########
# Support for zlib integration
!IF "$(ZLIB_SRC)" == ""
ZLIB_FLAGS =
ZLIB_LIBS =
!ELSE
ZLIB_FLAGS = /I "$(ZLIB_SRC)" /D NEON_ZLIB
!IF "$(ZLIB_DLL)" == ""
ZLIB_LIBS = "$(ZLIB_SRC)\zlibstat.lib"
!ELSE
ZLIB_FLAGS = $(ZLIB_FLAGS) /D ZLIB_DLL
ZLIB_LIBS = "$(ZLIB_SRC)\zlibdll.lib"
!ENDIF
!ENDIF


!IF "$(DEBUG_BUILD)" == ""
INTDIR = Release
CFLAGS = /MD /W3 /GX /O2 /D "NDEBUG"
TARGET = .\libneon.lib
!ELSE
INTDIR = Debug
CFLAGS = /MDd /W3 /Gm /GX /Zi /Od /D "_DEBUG"
TARGET = .\libneonD.lib
!ENDIF

# Exclude stuff we don't need from the Win32 headers
WIN32_DEFS = /D WIN32_LEAN_AND_MEAN /D NOUSER /D NOGDI /D NONLS /D NOCRYPT

CPP=cl.exe
CPP_PROJ = /c /nologo $(CFLAGS) $(WIN32_DEFS) $(EXPAT_FLAGS) $(OPENSSL_FLAGS) $(ZLIB_FLAGS) /D "HAVE_CONFIG_H" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\"
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(TARGET)"

LIB32_OBJS= \
	"$(INTDIR)\ne_alloc.obj" \
	"$(INTDIR)\ne_auth.obj" \
	"$(INTDIR)\ne_basic.obj" \
	"$(INTDIR)\ne_compress.obj" \
	"$(INTDIR)\ne_cookies.obj" \
	"$(INTDIR)\ne_dates.obj" \
	"$(INTDIR)\ne_i18n.obj" \
	"$(INTDIR)\ne_md5.obj" \
	"$(INTDIR)\ne_redirect.obj" \
	"$(INTDIR)\ne_request.obj" \
	"$(INTDIR)\ne_session.obj" \
	"$(INTDIR)\ne_socket.obj" \
	"$(INTDIR)\ne_string.obj" \
	"$(INTDIR)\ne_uri.obj" \
	"$(INTDIR)\ne_utils.obj"

!IF "$(BUILD_EXPAT)" != ""
LIB32_OBJS= \
	$(LIB32_OBJS) \
	"$(INTDIR)\ne_207.obj" \
	"$(INTDIR)\ne_xml.obj" \
	"$(INTDIR)\ne_acl.obj" \
	"$(INTDIR)\ne_props.obj" \
	"$(INTDIR)\ne_locks.obj" 
!ENDIF


!IF "$(OPENSSL_SRC)" != ""
LIB32_OBJS = $(LIB32_OBJS) "$(INTDIR)\ne_openssl.obj"
!IFDEF OPENSSL_STATIC
LIB32_OBJS = $(LIB32_OBJS) "$(OPENSSL_SRC)\out32\libeay32.lib" \
			   "$(OPENSSL_SRC)\out32\ssleay32.lib"
!ELSE
LIB32_OBJS = $(LIB32_OBJS) "$(OPENSSL_SRC)\out32dll\libeay32.lib" \
			   "$(OPENSSL_SRC)\out32dll\ssleay32.lib"
!ENDIF
!ELSE
# Provide ABI-compatibility stubs for SSL interface
LIB32_OBJS = $(LIB32_OBJS) "$(INTDIR)\ne_stubssl.obj"
!ENDIF
!IF "$(ZLIB_SRC)" != ""
LIB32_OBJS = $(LIB32_OBJS) $(ZLIB_LIBS)
!ENDIF


ALL: ".\src\config.h" "$(TARGET)"

CLEAN:
	-@erase "$(INTDIR)\ne_207.obj"
	-@erase "$(INTDIR)\ne_alloc.obj"
	-@erase "$(INTDIR)\ne_acl.obj"
	-@erase "$(INTDIR)\ne_auth.obj"
	-@erase "$(INTDIR)\ne_basic.obj"
	-@erase "$(INTDIR)\ne_compress.obj"
	-@erase "$(INTDIR)\ne_cookies.obj"
	-@erase "$(INTDIR)\ne_dates.obj"
	-@erase "$(INTDIR)\ne_i18n.obj"
	-@erase "$(INTDIR)\ne_locks.obj"
	-@erase "$(INTDIR)\ne_md5.obj"
	-@erase "$(INTDIR)\ne_props.obj"
	-@erase "$(INTDIR)\ne_redirect.obj"
	-@erase "$(INTDIR)\ne_request.obj"
	-@erase "$(INTDIR)\ne_session.obj"
	-@erase "$(INTDIR)\ne_openssl.obj"
	-@erase "$(INTDIR)\ne_stubssl.obj"
	-@erase "$(INTDIR)\ne_socket.obj"
	-@erase "$(INTDIR)\ne_string.obj"
	-@erase "$(INTDIR)\ne_uri.obj"
	-@erase "$(INTDIR)\ne_utils.obj"
	-@erase "$(INTDIR)\ne_xml.obj"
	-@erase "$(TARGET)"
	-@erase ".\src\config.h"

"$(TARGET)": $(DEF_FILE) $(LIB32_OBJS)
	-@if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"
	$(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

{src}.c{$(INTDIR)}.obj::
	-@if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"
	$(CPP) @<<
  $(CPP_PROJ) $<
<<

".\src\config.h": config.hw
	-@if not exist "$(INTDIR)/$(NULL)" mkdir "$(INTDIR)"
	<<tempfile.bat
  @echo off
  copy .\config.hw .\src\config.h > nul
  echo Created config.h from config.hw
<<

"$(INTDIR)\ne_207.obj":      .\src\ne_207.c
"$(INTDIR)\ne_alloc.obj":    .\src\ne_alloc.c
"$(INTDIR)\ne_acl.obj":      .\src\ne_acl.c
"$(INTDIR)\ne_auth.obj":     .\src\ne_auth.c
"$(INTDIR)\ne_basic.obj":    .\src\ne_basic.c
"$(INTDIR)\ne_compress.obj": .\src\ne_compress.c
"$(INTDIR)\ne_cookies.obj":  .\src\ne_cookies.c
"$(INTDIR)\ne_dates.obj":    .\src\ne_dates.c
"$(INTDIR)\ne_i18n.obj":     .\src\ne_i18n.c
"$(INTDIR)\ne_locks.obj":    .\src\ne_locks.c
"$(INTDIR)\ne_md5.obj":      .\src\ne_md5.c
"$(INTDIR)\ne_props.obj":    .\src\ne_props.c
"$(INTDIR)\ne_redirect.obj": .\src\ne_redirect.c
"$(INTDIR)\ne_request.obj":  .\src\ne_request.c
"$(INTDIR)\ne_session.obj":  .\src\ne_session.c
"$(INTDIR)\ne_openssl.obj":  .\src\ne_openssl.c
"$(INTDIR)\ne_stubssl.obj":  .\src\ne_stubssl.c
"$(INTDIR)\ne_socket.obj":   .\src\ne_socket.c
"$(INTDIR)\ne_string.obj":   .\src\ne_string.c
"$(INTDIR)\ne_uri.obj":      .\src\ne_uri.c
"$(INTDIR)\ne_utils.obj":    .\src\ne_utils.c
"$(INTDIR)\ne_xml.obj":      .\src\ne_xml.c
