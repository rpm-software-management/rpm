cmake_minimum_required(VERSION 3.10)

project(rpm
	VERSION 4.18.90
	DESCRIPTION "The RPM Package Manager"
	HOMEPAGE_URL "http://rpm.org"
	LANGUAGES C
)

# user configurable stuff
option(ENABLE_NLS "Enable native language support" ON)
option(ENABLE_OPENMP "Enable OpenMP threading support" ON)
option(ENABLE_PYTHON "Enable Python bindings" ON)
option(ENABLE_PLUGINS "Enable plugin support" ON)
option(ENABLE_WERROR "Stop build on warnings" OFF)
option(ENABLE_SQLITE "Enable sqlite rpmdb support" ON)
option(ENABLE_NDB "Enable ndb rpmdb support" ON)
option(ENABLE_BDB_RO "Enable read-only Berkeley DB rpmdb support (EXPERIMENTAL)" OFF)

option(WITH_INTERNAL_OPENPGP "Use internal OpenPGP parse (DEPRECATED)" ON)
option(WITH_OPENSSL "Use openssl (instead of libgcrypt) for internal crypto" OFF)

option(WITH_CAP "Build with capability support" ON)
option(WITH_ACL "Build with ACL support" ON)
option(WITH_ARCHIVE "Build with libarchive support" ON)
option(WITH_SELINUX "Build with SELinux support" ON)
option(WITH_DBUS "Build with DBUS support" ON)
option(WITH_AUDIT "Build with audit support" ON)
option(WITH_FSVERITY "Build with fsverity support" OFF)
option(WITH_IMAEVM "Build with IMA support" OFF)

set(RPMCONFIGDIR "${CMAKE_INSTALL_PREFIX}/lib/rpm" CACHE PATH "rpm home")
set(RPMCANONVENDOR "vendor" CACHE STRING "rpm vendor string")

# emulate libtool versioning
set(RPM_SOVERSION 9)
set(RPM_LIBVERSION ${RPM_SOVERSION}.3.0)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_SHARED_MODULE_PREFIX "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(GNUInstallDirs)
add_compile_definitions(_GNU_SOURCE)

# autotools compat stuff
set(PACKAGE "${PROJECT_NAME}")
set(PACKAGE_NAME "${PROJECT_NAME}")
set(VERSION "${PROJECT_VERSION}")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(PACKAGE_BUGREPORT "${PROJECT_HOMEPAGE_URL}")

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
	set(infodir "\${datarootdir}/${CMAKE_INSTALL_INFODIR}")
	set(mandir "\${datarootdir}/${CMAKE_INSTALL_MANDIR}")
	set(RUNDIR /run)

	set(acutils
		awk ar as cpp cxx
	)
	foreach (V ${acutils})
		string(TOUPPER ${V} n)
		set(${n} /usr/bin/${V})
	endforeach()
	set(extutils
		7zip bzip2 cat chmod chown cp curl file gpg grep gzip id cc ln
		install lrzip lzip xz make mkdir mv patch rm sed tar unzip
		zstd gem git hg bzr quilt ld objdump strip
	)
	foreach (V ${extutils})
		string(TOUPPER ${V} n)
		set(__${n} /usr/bin/${V})
	endforeach()

	set(DB_BACKEND sqlite)

	set(host_cpu ${CMAKE_HOST_SYSTEM_PROCESSOR})
	string(TOLOWER ${CMAKE_HOST_SYSTEM_NAME} host_os)
	set(host_vendor redhat)
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
		${CMAKE_COMMAND} -E env pkglibdir=${RPMCONFIGDIR}
			${CMAKE_SOURCE_DIR}/installplatform
			rpmrc platform macros
			${RPMCANONVENDOR} ${RPMCANONOS} ${RPMCANONGNU})"
	)
endfunction()

include(CheckLibraryExists)
include(CheckFunctionExists)
include(CheckIncludeFile)

find_package(PkgConfig REQUIRED)
find_package(ZLIB REQUIRED)
find_package(BZip2)
find_package(LibLZMA)
find_package(Iconv)

pkg_check_modules(LUA lua REQUIRED)
pkg_check_modules(POPT popt REQUIRED)
pkg_check_modules(MAGIC libmagic REQUIRED)
pkg_check_modules(READLINE readline)
pkg_check_modules(ZSTD libzstd)
pkg_check_modules(LIBELF libelf)
pkg_check_modules(LIBDW libdw)

if (ENABLE_OPENMP)
	find_package(OpenMP REQUIRED)
endif()

if (ENABLE_NLS)
	find_package(Intl REQUIRED)
endif()

if (ENABLE_SQLITE)
	pkg_check_modules(SQLITE sqlite3 REQUIRED)
endif()

if (ENABLE_PYTHON)
	find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
endif()

if (WITH_CAP)
	pkg_check_modules(LIBCAP libcap REQUIRED)
endif()

if (WITH_SELINUX)
	pkg_check_modules(SELINUX libselinux REQUIRED)
endif()

if (WITH_ARCHIVE)
	pkg_check_modules(LIBARCHIVE libarchive REQUIRED)
endif()

if (WITH_FSVERITY)
	pkg_check_modules(FSVERITY libfsverity REQUIRED)
endif()

if (NOT WITH_INTERNAL_OPENPGP)
	pkg_check_modules(RPMSEQUOIA rpm-sequoia REQUIRED)
endif()

find_program(__FIND_DEBUGINFO find-debuginfo)

function(chkdef func req)
	string(TOUPPER ${func} FUNC)
	set(HAVENAME HAVE_${FUNC})
	check_function_exists(${func} ${HAVENAME})
	if (${req} AND NOT ${HAVENAME})
		message(FATAL_ERROR "required function ${func} not found")
	endif()
endfunction()

set(OPTFUNCS
	stpcpy stpncpy putenv mempcpy fdatasync lutimes mergesort
	getauxval setprogname __progname syncfs sched_getaffinity unshare
	secure_getenv __secure_getenv mremap)
foreach(f ${OPTFUNCS})
    chkdef(${f} FALSE)
endforeach()

set(REQFUNCS
	mkstemp getcwd basename dirname realpath setenv unsetenv regcomp
	utimes getline localtime_r statvfs getaddrinfo
	openat mkdirat fstatat linkat symlinkat mkfifoat mknodat unlinkat
	renameat utimensat fchmodat fchownat stpcpy stpncpy
)
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
	execute_process(COMMAND awk -F: "$3==0 {print $1;exit}" ${file}
			OUTPUT_STRIP_TRAILING_WHITESPACE
			OUTPUT_VARIABLE name)
	if ("${name}" STREQUAL "")
		set(name foor)
	endif()
	set(${var} ${name} PARENT_SCOPE)
endfunction()

id0name(UID_0_USER /etc/passwd)
id0name(GID_0_GROUP /etc/group)

# map module/package findings to config.h
if (${Bzip2_FOUND})
	set(HAVE_BZLIB_H 1)
endif()
if (${LibLZMA_FOUND})
	set(HAVE_LZMA_H 1)
endif()
if (${Iconv_FOUND})
	set(HAVE_ICONV 1)
endif()
foreach(found ZSTD READLINE LIBELF LIBDW)
	if (${${found}_FOUND})
		set(HAVE_${found} 1)
	endif()
endforeach()

configure_file(config.h.in config.h)

add_compile_definitions(HAVE_CONFIG_H)

add_compile_definitions(MAJOR_IN_SYSMACROS)

add_compile_definitions(RPMCONFIGDIR="${RPMCONFIGDIR}")
add_compile_definitions(LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")
add_compile_definitions(LOCALSTATEDIR="${CMAKE_INSTALL_FULL_LOCALSTATEDIR}")
add_compile_definitions(SYSCONFDIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}")
add_compile_definitions(LIBRPMALIAS_FILENAME="rpmpopt-${VERSION}")
add_compile_definitions(LIBRPMALIAS_EXECPATH="${CMAKE_INSTALL_FULL_BINDIR}")

if (ENABLE_WERROR)
	add_compile_options(-Werror)
endif()

include_directories(${CMAKE_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include)

add_subdirectory(docs)
add_subdirectory(include/rpm)
add_subdirectory(misc)
add_subdirectory(rpmio)
add_subdirectory(lib)
add_subdirectory(build)
add_subdirectory(sign)
add_subdirectory(po)

if (ENABLE_PYTHON)
	add_subdirectory(python)
endif()

if (ENABLE_PLUGINS)
	add_subdirectory(plugins)
endif()


add_subdirectory(fileattrs)
add_subdirectory(scripts)

add_library(cliutils OBJECT cliutils.c cliutils.h)

set(rpmbuild_libs librpmbuild)
set(rpmspec_libs librpmbuild ${READLINE_LIBRARIES})
set(rpmsign_libs librpmsign)

set(rpmlibs librpmio librpm ${POPT_LIBRARIES})
set(rpmcli rpm rpmdb rpmkeys rpm2cpio rpmbuild rpmspec rpmsign)
if (WITH_ARCHIVE)
	set(rpm2archive_libs ${LIBARCHIVE_LIBRARIES})
	list(APPEND rpmcli rpm2archive)
endif()

foreach(cmd ${rpmcli})
	add_executable(${cmd} ${cmd}.c cliutils)
	target_link_libraries(${cmd} PRIVATE ${rpmlibs} ${${cmd}_libs})
endforeach()
install(TARGETS ${rpmcli})

foreach(cmd rpmverify rpmquery)
	add_custom_target(${cmd} ALL COMMAND
			${CMAKE_COMMAND} -E create_symlink rpm ${cmd})
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${cmd} TYPE BIN)
endforeach()

set(rpmdeps_libs librpmbuild)
set(rpmlua_libs ${READLINE_LIBRARIES})
set(rpmtools rpmdeps rpmgraph rpmlua rpmuncompress)
if (${LIBELF_FOUND})
	set(elfdeps_libs ${LIBELF_LIBRARIES})
	list(APPEND rpmtools elfdeps)
endif()
foreach(cmd ${rpmtools})
	add_executable(${cmd} tools/${cmd}.c)
	target_link_libraries(${cmd} PRIVATE ${rpmlibs} ${${cmd}_libs})
endforeach()
install(TARGETS elfdeps rpmdeps rpmuncompress DESTINATION ${RPMCONFIGDIR})
install(TARGETS rpmgraph rpmlua)

makemacros()
foreach(f macros rpmrc rpmpopt-${PROJECT_VERSION})
	install(FILES ${CMAKE_BINARY_DIR}/${f} DESTINATION ${RPMCONFIGDIR})
endforeach()

find_program(AUTOM4TE autom4te)
find_program(FAKECHROOT fakechroot)
if (DEFINED AUTOM4TE AND DEFINED FAKECHROOT)
	add_subdirectory(tests)
endif()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/rpm.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(DIRECTORY DESTINATION ${RPMCONFIGDIR}/lua)
install(DIRECTORY DESTINATION ${RPMCONFIGDIR}/macros.d)
install(FILES CONTRIBUTING.md COPYING CREDITS INSTALL README TYPE DOC)
