set(RPM_TARGETS librpm librpmio librpmbuild librpmsign)

function(os_release var key)
	execute_process(
		COMMAND sh -c "grep ^${key} /etc/os-release | cut -d= -f2"
		OUTPUT_STRIP_TRAILING_WHITESPACE
		OUTPUT_VARIABLE value
	)
	string(TOLOWER ${value} value)
	set(${var} ${value} PARENT_SCOPE)
endfunction()

os_release(OS_NAME ID)
os_release(OS_VERSION VERSION_ID)

set(PYTHON ${Python3_EXECUTABLE})
if (${WITH_INTERNAL_OPENPGP})
	if (${WITH_OPENSSL})
		set(CRYPTO openssl)
	else()
		set(CRYPTO libgcrypt)
	endif()
else()
	set(CRYPTO sequoia)
endif()

set(TESTSUITE_AT
	rpmtests.at
	rpmgeneral.at
	rpmquery.at
	rpmverify.at
	rpmdb.at
	rpmbuild.at
	rpmbuildid.at
	rpmi.at
	rpme.at
	rpmvercmp.at
	rpmdeps.at
	rpmconflict.at
	rpmconfig.at
	rpmconfig2.at
	rpmconfig3.at
	rpmreplace.at
	rpmmacro.at
	rpmpython.at
	rpmdepmatch.at
	rpmscript.at
	rpmsigdig.at
	rpmspec.at
	rpmio.at
	rpmorder.at
	rpmvfylevel.at
	rpmpgp.at
)

find_program(AUTOM4TE autom4te REQUIRED)
set(AUTOTEST ${AUTOM4TE} --language=autotest)

set(TESTPROGS rpmpgpcheck rpmpgppubkeyfingerprint)
foreach(prg ${TESTPROGS})
	add_executable(${prg} EXCLUDE_FROM_ALL ${prg}.c)
	target_link_libraries(${prg} PRIVATE librpmio)
endforeach()
string(REPLACE ";" " " TESTPROG_NAMES "${TESTPROGS}")

include(ProcessorCount)
ProcessorCount(nproc)
if (nproc GREATER 1)
	set(JOBS -j${nproc})
endif()

find_program(BWRAP bwrap REQUIRED)

set(MKTREE_BACKEND oci CACHE STRING "Mktree backend to use")
set(MKTREE_NATIVE ON CACHE BOOL "Mktree native mode")

message(STATUS "Using mktree backend: ${MKTREE_BACKEND}")

# Set up mktree
if ("${MKTREE_BACKEND}" STREQUAL "oci")
	find_program(PODMAN podman REQUIRED)
	find_program(BUILDAH buildah REQUIRED)
	if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Dockerfile.${OS_NAME})
		# Native building supported
		configure_file(Dockerfile.${OS_NAME} Dockerfile COPYONLY)
	else()
		# Fall back to containerized building
		set(MKTREE_NATIVE OFF)
		configure_file(Dockerfile Dockerfile COPYONLY)
	endif()
	message(STATUS "Native mktree mode: ${MKTREE_NATIVE}")
	set(MKTREE_BUILD_DEPS Dockerfile)
	add_custom_target(ci
		COMMAND ./mktree build --full
		COMMAND ./mktree check ${JOBS} $(TESTOPTS)
	)
endif()
configure_file(mktree.${MKTREE_BACKEND} mktree @ONLY)
configure_file(mktree.common mktree.common @ONLY)
configure_file(mktree.rootfs mktree.rootfs @ONLY)

configure_file(atlocal.in atlocal @ONLY)
configure_file(package.m4.in package.m4 @ONLY)

add_custom_target(check
	COMMAND ./mktree check ${JOBS} $(TESTOPTS)
	DEPENDS mktree.dir
)

add_custom_target(shell
	COMMAND ./mktree shell ||:
	DEPENDS mktree.dir
)

add_custom_target(atshell
	COMMAND ./mktree atshell ||:
	DEPENDS mktree.dir
)

add_custom_target(reset
	COMMAND ./mktree reset
)

add_custom_target(tree
	DEPENDS mktree.dir
)

add_custom_command(OUTPUT rpmtests
	COMMAND ${AUTOTEST} -I ${CMAKE_CURRENT_SOURCE_DIR}
		-o rpmtests rpmtests.at
	DEPENDS ${TESTSUITE_AT}
	DEPENDS local.at
)

add_custom_command(OUTPUT mktree.dir
	COMMAND ./mktree build
	DEPENDS rpmtests
	DEPENDS ${TESTPROGS}
	DEPENDS ${RPM_TARGETS}
	DEPENDS ${MKTREE_BUILD_DEPS}
)
