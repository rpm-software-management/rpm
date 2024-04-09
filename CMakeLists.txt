cmake_minimum_required(VERSION 3.18)

project(rpm
	VERSION 5.99.90
	DESCRIPTION "The RPM Package Manager"
	HOMEPAGE_URL "http://rpm.org"
	LANGUAGES C CXX
)

# user configurable stuff
option(ENABLE_CUTF8 "Enable C.UTF-8 as default locale" ON)
option(ENABLE_NLS "Enable native language support" ON)
option(ENABLE_OPENMP "Enable OpenMP threading support" ON)
option(ENABLE_PYTHON "Enable Python bindings" ON)
option(ENABLE_PLUGINS "Enable plugin support" ON)
option(ENABLE_WERROR "Stop build on warnings" OFF)
option(ENABLE_SQLITE "Enable sqlite rpmdb support" ON)
option(ENABLE_NDB "Enable ndb rpmdb support" ON)
option(ENABLE_BDB_RO "Enable read-only Berkeley DB rpmdb support (EXPERIMENTAL)" OFF)
option(ENABLE_TESTSUITE "Enable test-suite" ON)
option(ENABLE_ASAN "Enable address-sanitizer" OFF)
option(ENABLE_UBSAN "Enable undefined behavior-sanitizer" OFF)

option(WITH_CAP "Build with capability support" ON)
option(WITH_ACL "Build with ACL support" ON)
option(WITH_ARCHIVE "Build with libarchive support" ON)
option(WITH_SELINUX "Build with SELinux support" ON)
option(WITH_DBUS "Build with DBUS support" ON)
option(WITH_AUDIT "Build with audit support" ON)
option(WITH_FSVERITY "Build with fsverity support" OFF)
option(WITH_IMAEVM "Build with IMA support" OFF)
option(WITH_FAPOLICYD "Build with fapolicyd support" ON)
option(WITH_SEQUOIA "Build with Sequoia OpenPGP support" ON)
option(WITH_OPENSSL "Use openssl instead of libgcrypt for internal crypto" OFF)
option(WITH_READLINE "Build with readline support" ON)
option(WITH_BZIP2 "Build with bzip2 support" ON)
option(WITH_ICONV "Build with iconv support" ON)
option(WITH_ZSTD "Build with zstd support" ON)
option(WITH_LIBDW "Build with libdw support" ON)
option(WITH_LIBELF "Build with libelf support" ON)
option(WITH_LIBLZMA "Build with liblzma support" ON)
option(WITH_DOXYGEN "Build API docs with doxygen" OFF)

set(RPM_CONFIGDIR "${CMAKE_INSTALL_PREFIX}/lib/rpm" CACHE PATH "rpm home")
set(RPM_MACROSDIR "${RPM_CONFIGDIR}/macros.d")
set(RPM_VENDOR "vendor" CACHE STRING "rpm vendor string")

# Emulate libtool versioning. Before a public release:
# - increment micro version whenever there are code changes
# - increment minor version whenever there are added interfaces
# - increment major version whenever there are removed interfaces
# - incrementing a more significant version segment resets changes to
#   any less significant segments
set(RPM_SOVERSION 10)
set(RPM_LIBVERSION ${RPM_SOVERSION}.0.0)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# extensions needed for typeof() hacks during transition
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_SHARED_MODULE_PREFIX "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(GNUInstallDirs)
include(CheckSymbolExists)
add_compile_definitions(_GNU_SOURCE)
add_definitions(-D_FILE_OFFSET_BITS=64)

# Find a (system) utility in canonical FHS paths, excluding $PATH.  Useful for
# when the path is to be used at RPM runtime, such as in the macro files.  The
# MYPATH environment variable can be used to augment these paths.
function(findutil UTIL TRY)
	list(GET TRY 0 util)
	find_program(${UTIL}
		NAMES ${TRY}
		PATHS ENV MYPATH
		PATHS /usr/local/bin /usr/bin /bin
		PATHS /usr/local/sbin /usr/sbin /sbin
		NO_DEFAULT_PATH
	)
	if (NOT ${UTIL})
		list(GET TRY 0 util)
		message(DEBUG "${util} not found, assuming /usr/bin/${util}")
		set(${UTIL} /usr/bin/${util} PARENT_SCOPE)
	endif()
	mark_as_advanced(${UTIL})
endfunction()

function(makemacros)
	set(prefix ${CMAKE_INSTALL_PREFIX})
	set(exec_prefix "\${prefix}")
	set(bindir "\${exec_prefix}/${CMAKE_INSTALL_BINDIR}")
	set(sbindir "\${exec_prefix}/${CMAKE_INSTALL_SBINDIR}")
	set(libexecdir "\${exec_prefix}/${CMAKE_INSTALL_LIBEXECDIR}")
	set(datarootdir "\${prefix}/${CMAKE_INSTALL_DATAROOTDIR}")
	set(datadir "\${datarootdir}")
	set(sysconfdir "${CMAKE_INSTALL_FULL_SYSCONFDIR}")
	set(sharedstatedir "${CMAKE_INSTALL_FULL_SHAREDSTATEDIR}")
	set(localstatedir "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}")
	set(libdir "\${prefix}/=LIB=")
	set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
	set(oldincludedir "${CMAKE_INSTALL_FULL_OLDINCLUDEDIR}")
	set(infodir "\${prefix}/${CMAKE_INSTALL_INFODIR}")
	set(mandir "\${prefix}/${CMAKE_INSTALL_MANDIR}")
	set(rundir /run)

	pkg_get_variable(sysusersdir systemd sysusersdir)
	if (NOT sysusersdir)
		set(sysusersdir /usr/lib/sysusers.d)
	endif()

	findutil(__7ZIP "7za;7z")
	findutil(__BZIP2 bzip2)
	findutil(__CAT cat)
	findutil(__CHMOD chmod)
	findutil(__CHOWN chown)
	findutil(__CP cp)
	findutil(__CURL curl)
	findutil(__FILE file)
	findutil(__GPG gpg)
	findutil(__GREP grep)
	findutil(__GZIP gzip)
	findutil(__ID id)
	findutil(__CC cc)
	findutil(__LN ln)
	findutil(__INSTALL install)
	findutil(__LRZIP lrzip)
	findutil(__LZIP lzip)
	findutil(__XZ xz)
	findutil(__MAKE make)
	findutil(__MKDIR mkdir)
	findutil(__MV mv)
	findutil(__PATCH patch)
	findutil(__RM rm)
	findutil(__SED sed)
	findutil(__TAR tar)
	findutil(__UNZIP unzip)
	findutil(__ZSTD zstd)
	findutil(__GEM gem)
	findutil(__GIT git)
	findutil(__HG hg)
	findutil(__BZR bzr)
	findutil(__QUILT quilt)
	findutil(__LD ld)
	findutil(__OBJDUMP objdump)
	findutil(__STRIP strip)
	findutil(__SYSTEMD_SYSUSERS systemd-sysusers)
	findutil(__FIND_DEBUGINFO find-debuginfo)
	findutil(__AWK awk)
	findutil(__AR ar)
	findutil(__AS as)
	findutil(__CPP cpp)
	findutil(__CXX c++)

	list(GET db_backends 0 DB_BACKEND)

	set(host_cpu ${CMAKE_HOST_SYSTEM_PROCESSOR})
	string(TOLOWER ${CMAKE_HOST_SYSTEM_NAME} host_os)
	set(host_vendor ${RPM_VENDOR})
	set(host ${host_cpu}-${host_vendor}-${host_os})

	set(RPMCANONVENDOR ${host_vendor})
	set(RPMCANONOS ${host_os})
	set(RPMCANONGNU -gnu)

	configure_file(platform.in platform @ONLY)
	configure_file(rpmrc.in rpmrc @ONLY)
	configure_file(macros.in macros @ONLY)
	configure_file(rpmpopt.in rpmpopt-${PROJECT_VERSION} @ONLY)
	configure_file(rpm.pc.in rpm.pc @ONLY)

	install(CODE "execute_process(COMMAND
		${CMAKE_COMMAND} -E env pkglibdir=${RPM_CONFIGDIR}
			${CMAKE_SOURCE_DIR}/installplatform
			rpmrc platform macros
			${RPMCANONVENDOR} ${RPMCANONOS} ${RPMCANONGNU}
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR})"
	)
endfunction()

include(CheckLibraryExists)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckCCompilerFlag)
include(CheckVariableExists)

set(OPTFUNCS
	stpcpy stpncpy putenv mempcpy fdatasync lutimes mergesort
	getauxval setprogname __progname syncfs sched_getaffinity unshare
	secure_getenv __secure_getenv mremap strchrnul
)
set(REQFUNCS
	mkstemp getcwd basename dirname realpath setenv unsetenv regcomp
	utimes getline localtime_r statvfs getaddrinfo
	openat mkdirat fstatat linkat symlinkat mkfifoat mknodat unlinkat
	renameat utimensat fchmodat fchownat stpcpy stpncpy
)

find_package(PkgConfig REQUIRED)
find_package(Lua 5.2 REQUIRED)
find_package(ZLIB REQUIRED)
if (WITH_BZIP2)
    find_package(BZip2 REQUIRED)
endif()
if (WITH_ICONV)
    find_package(Iconv REQUIRED)
endif()

pkg_check_modules(POPT REQUIRED IMPORTED_TARGET popt)

if (WITH_READLINE)
    pkg_check_modules(READLINE REQUIRED IMPORTED_TARGET readline)
endif()

if (WITH_ZSTD)
    pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd>=1.3.8)
endif()
if (WITH_LIBELF)
    pkg_check_modules(LIBELF REQUIRED IMPORTED_TARGET libelf)
endif()
if (WITH_LIBDW)
    pkg_check_modules(LIBDW REQUIRED IMPORTED_TARGET libdw)
endif()
if (WITH_LIBLZMA)
    pkg_check_modules(LIBLZMA REQUIRED IMPORTED_TARGET liblzma>=5.2.0)
endif()
pkg_check_modules(LIBARCHIVE REQUIRED IMPORTED_TARGET libarchive)

# Lua module does not ship an IMPORTED target, define our own
add_library(LUA::LUA INTERFACE IMPORTED)
set_target_properties(LUA::LUA PROPERTIES
		      INTERFACE_LINK_LIBRARIES "${LUA_LIBRARIES}"
		      INTERFACE_INCLUDE_DIRECTORIES "${LUA_INCLUDE_DIR}")

# file >= 5.39 ships a pkg-config, may move to that later
add_library(MAGIC::MAGIC UNKNOWN IMPORTED)
find_library(MAGIC_LIBRARY NAMES magic REQUIRED)
find_path(MAGIC_INCLUDE_DIR NAMES magic.h REQUIRED)
mark_as_advanced(MAGIC_LIBRARY)
mark_as_advanced(MAGIC_INCLUDE_DIR)
set_target_properties(MAGIC::MAGIC PROPERTIES
		      IMPORTED_LOCATION "${MAGIC_LIBRARY}")
target_include_directories(MAGIC::MAGIC INTERFACE "${MAGIC_INCLUDE_DIR}")

if (ENABLE_OPENMP)
	find_package(OpenMP 4.5 REQUIRED)
endif()

if (ENABLE_NLS)
	find_package(Intl REQUIRED)
	check_variable_exists(_nl_msg_cat_cntr HAVE_NL_MSG_CAT_CNTR)
endif()

if (ENABLE_SQLITE)
	pkg_check_modules(SQLITE REQUIRED IMPORTED_TARGET sqlite3>=3.22)
	list(APPEND db_backends sqlite)
endif()

if (ENABLE_NDB)
	list(APPEND db_backends ndb)
endif()

if (ENABLE_BDB_RO)
	list(APPEND db_backends bdb_ro)
endif()
list(APPEND db_backends dummy)

if (ENABLE_PYTHON)
	find_package(Python3 3.7 COMPONENTS Interpreter Development REQUIRED)
endif()

if (WITH_CAP)
	pkg_check_modules(LIBCAP REQUIRED IMPORTED_TARGET libcap)
endif()

if (WITH_ACL)
	pkg_check_modules(LIBACL REQUIRED IMPORTED_TARGET libacl)
endif()

if (WITH_AUDIT)
	pkg_check_modules(AUDIT REQUIRED IMPORTED_TARGET audit)
endif()

if (WITH_SELINUX)
	pkg_check_modules(SELINUX REQUIRED IMPORTED_TARGET libselinux)
endif()

if (WITH_FSVERITY)
	pkg_check_modules(FSVERITY REQUIRED IMPORTED_TARGET libfsverity)
endif()

if (WITH_IMAEVM)
	list(APPEND REQFUNCS lsetxattr)
	add_library(IMA::IMA UNKNOWN IMPORTED)
	find_path(IMA_INCLUDE_DIR NAMES imaevm.h REQUIRED)
	find_library(IMA_LIBRARY NAMES imaevm REQUIRED)
	mark_as_advanced(IMA_INCLUDE_DIR)
	mark_as_advanced(IMA_LIBRARY)
	set_target_properties(IMA::IMA PROPERTIES
			      IMPORTED_LOCATION "${IMA_LIBRARY}")
	target_include_directories(IMA::IMA INTERFACE "${IMA_INCLUDE_DIR}")
endif()

find_program(AWK gawk mawk nawk awk REQUIRED)
mark_as_advanced(AWK)

find_program(FIND_DEBUGINFO find-debuginfo)
mark_as_advanced(FIND_DEBUGINFO)

function(chkdef func req)
	string(TOUPPER ${func} FUNC)
	set(HAVENAME HAVE_${FUNC})
	check_function_exists(${func} ${HAVENAME})
	if (${req} AND NOT ${HAVENAME})
		message(FATAL_ERROR "required function ${func} not found")
	endif()
endfunction()

foreach(f ${OPTFUNCS})
    chkdef(${f} FALSE)
endforeach()

foreach(f ${REQFUNCS})
    chkdef(${f} TRUE)
endforeach()

function(chkhdr inc req)
	string(MAKE_C_IDENTIFIER ${inc} ID)
	string(TOUPPER ${ID} INC)
	set(HAVENAME HAVE_${INC})
	check_include_file(${inc} ${HAVENAME})
	if (${req} AND NOT ${HAVENAME})
		message(FATAL_ERROR "required include ${inc} not found")
	endif()
endfunction()

set(OPTINCS
	unistd.h limits.h getopt.h
	sys/utsname.h sys/systemcfg.h sys/param.h sys/auxv.h
)
foreach(f ${OPTINCS})
    chkhdr(${f} FALSE)
endforeach()

function(id0name var file)
	execute_process(COMMAND ${AWK} -F: "$3==0 {print $1;exit}" ${file}
			OUTPUT_STRIP_TRAILING_WHITESPACE
			OUTPUT_VARIABLE name)
	if ("${name}" STREQUAL "")
		set(name root)
	endif()
	set(${var} ${name} PARENT_SCOPE)
endfunction()

id0name(UID_0_USER /etc/passwd)
id0name(GID_0_GROUP /etc/group)

# map module/package findings to config.h
if (BZIP2_FOUND)
	set(HAVE_BZLIB_H 1)
endif()
if (LIBLZMA_FOUND)
	set(HAVE_LZMA_H 1)
endif()
if (Iconv_FOUND)
	set(HAVE_ICONV 1)
endif()
if (ZSTD_FOUND)
	set(HAVE_ZSTD 1)
endif()
if (READLINE_FOUND)
	set(HAVE_READLINE 1)
endif()
if (LIBELF_FOUND)
	set(HAVE_LIBELF 1)
endif()
if (LIBDW_FOUND)
	set(HAVE_LIBDW 1)
endif()

check_symbol_exists(GLOB_ONLYDIR "glob.h" HAVE_GLOB_ONLYDIR)
check_symbol_exists(major "sys/sysmacros.h" MAJOR_IN_SYSMACROS)
if (NOT MAJOR_IN_SYSMACROS)
	check_symbol_exists(major "sys/mkdev.h" MAJOR_IN_MKDEV)
endif()

if (ENABLE_CUTF8)
	set(C_LOCALE "C.UTF-8")
else()
	set(C_LOCALE "C")
endif()
message(INFO " using ${C_LOCALE} as default locale")

configure_file(config.h.in config.h)

add_compile_definitions(HAVE_CONFIG_H)

add_compile_options(-Wall -Wpointer-arith -Wempty-body)
if (ENABLE_WERROR)
	add_compile_options(-Werror)
endif()

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -Wno-c++20-compat -Wno-sign-compare")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-prototypes -Wstrict-prototypes")

if (ENABLE_ASAN)
	add_compile_options(-fsanitize=address)
	add_link_options(-fsanitize=address)
endif()

if (ENABLE_UBSAN)
	add_compile_options(-fsanitize=undefined)
	add_link_options(-fsanitize=undefined)
endif()

if (ENABLE_ASAN OR ENABLE_UBSAN)
	add_compile_options(-fno-omit-frame-pointer)
endif()

# try to ensure some compiler sanity
foreach (flag -fno-strict-overflow -fno-delete-null-pointer-checks)
	check_c_compiler_flag(${flag} found)
	if (found)
		add_compile_options(${flag})
	endif()
	unset(found)
endforeach()

# generated sources
include_directories(${CMAKE_BINARY_DIR})
# global private includes
include_directories(${CMAKE_SOURCE_DIR}/misc)
# public headers
include_directories(${CMAKE_SOURCE_DIR}/include)

add_subdirectory(docs)
add_subdirectory(include/rpm)
add_subdirectory(misc)
add_subdirectory(rpmio)
add_subdirectory(lib)
add_subdirectory(build)
add_subdirectory(sign)
add_subdirectory(tools)

if (EXISTS ${CMAKE_SOURCE_DIR}/po/rpm.pot)
	add_subdirectory(po)
endif()

if (ENABLE_PYTHON)
	add_subdirectory(python)
endif()

if (ENABLE_PLUGINS)
	add_subdirectory(plugins)
endif()


add_subdirectory(fileattrs)
add_subdirectory(scripts)

makemacros()
foreach(f macros rpmrc rpmpopt-${PROJECT_VERSION})
	install(FILES ${CMAKE_BINARY_DIR}/${f} DESTINATION ${RPM_CONFIGDIR})
endforeach()

if (ENABLE_TESTSUITE)
	add_subdirectory(tests)
endif()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/rpm.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(DIRECTORY DESTINATION ${RPM_CONFIGDIR}/lua)
install(DIRECTORY DESTINATION ${RPM_CONFIGDIR}/macros.d)
install(FILES CONTRIBUTING.md COPYING CREDITS INSTALL README TYPE DOC)

add_custom_target(ChangeLog
		   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		   COMMAND git log --no-merges --no-decorate
				--output=${CMAKE_BINARY_DIR}/ChangeLog
)

add_custom_command(OUTPUT po/rpm.pot
		   COMMAND git submodule update --init)

function(add_tarball targetname namever treeish)
	set(distfmt tar)
	set(tarname ${namever}.${distfmt})
	set(distname ${tarname}.bz2)
	set(docname ${namever}-doc.${distfmt})

	add_custom_target(${docname}
		DEPENDS man apidoc
		COMMAND tar
			-C ${CMAKE_BINARY_DIR}
			--transform 's:^:${namever}/:'
			-cf ${docname} docs/man/*.[1-8] docs/html/
	)

	add_custom_target(${distname}
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		VERBATIM
		DEPENDS ChangeLog ${docname} po/rpm.pot
		COMMAND git archive
			--format=${distfmt}
			--output=${CMAKE_BINARY_DIR}/${tarname}
			--prefix=${namever}/
			--add-file=${CMAKE_BINARY_DIR}/ChangeLog
			${treeish}
		COMMAND git submodule foreach --quiet
			"git archive --prefix=${namever}/$sm_path/ \
				--output=${CMAKE_BINARY_DIR}/$sha1.tar HEAD \
			 && tar --concatenate \
				--file=${CMAKE_BINARY_DIR}/${tarname} \
				${CMAKE_BINARY_DIR}/$sha1.tar \
			 && rm -f ${CMAKE_BINARY_DIR}/$sha1.tar"
		COMMAND tar --concatenate
				--file=${CMAKE_BINARY_DIR}/${tarname}
				${CMAKE_BINARY_DIR}/${docname}
		COMMAND bzip2 -f ${CMAKE_BINARY_DIR}/${tarname}
	)
	add_custom_target(${targetname} DEPENDS ${distname})
endfunction()

add_tarball(dist ${PROJECT_NAME}-${PROJECT_VERSION} HEAD)

if (EXISTS ${CMAKE_SOURCE_DIR}/.git)
	execute_process(COMMAND git rev-list --count HEAD
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
			OUTPUT_VARIABLE gitcount
			OUTPUT_STRIP_TRAILING_WHITESPACE)
	add_tarball(snapshot
		    ${PROJECT_NAME}-${PROJECT_VERSION}-git${gitcount} HEAD)
endif()

# Voodoo rites to make our libraries cmake find_package() importable
include(CMakePackageConfigHelpers)
configure_package_config_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/rpm-config.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/rpm-config.cmake
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rpm
)
write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/rpm-config-version.cmake
	COMPATIBILITY SameMinorVersion
)
install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/rpm-config.cmake
	${CMAKE_CURRENT_BINARY_DIR}/rpm-config-version.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rpm
)

export(TARGETS librpm librpmio librpmbuild librpmsign
	FILE rpm-targets.cmake
	NAMESPACE rpm::
)
install(EXPORT rpm-targets
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CMAKE_PROJECT_NAME}
	NAMESPACE rpm::
)
