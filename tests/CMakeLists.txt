# Use our top-level targets as an ordering clue to cmake: the project
# needs to be built before we can populate anything...
get_property(TOP_TARGETS DIRECTORY .. PROPERTY BUILDSYSTEM_TARGETS)

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

find_program(BWRAP bwrap REQUIRED)
find_program(AUTOM4TE autom4te REQUIRED)
set(AUTOTEST ${AUTOM4TE} --language=autotest)

set(TESTPROGS rpmpgpcheck rpmpgppubkeyfingerprint)
foreach(prg ${TESTPROGS})
	add_executable(${prg} EXCLUDE_FROM_ALL ${prg}.c)
	target_link_libraries(${prg} PRIVATE librpmio)
endforeach()
string(REPLACE ";" " " TESTPROG_NAMES "${TESTPROGS}")

configure_file(atlocal.in atlocal @ONLY)
configure_file(package.m4.in package.m4 @ONLY)

# Detect suitable mktree backend
if ("${MKTREE_BACKEND}" STREQUAL "")
	if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/mktree.${OS_NAME})
		set(MKTREE_BACKEND ${OS_NAME})
	else()
		message(WARNING
			"No suitable mktree backend found for this platform. "
			"Disabling test-suite.")
		return()
	endif()
endif()
message(STATUS "Using mktree backend: ${MKTREE_BACKEND}")
configure_file(mktree.common mktree.common @ONLY)
configure_file(mktree.native mktree.native @ONLY)
configure_file(mktree.${MKTREE_BACKEND} mktree @ONLY)

include(ProcessorCount)
ProcessorCount(nproc)
if (nproc GREATER 1)
	set(JOBS -j${nproc})
endif()

add_custom_target(env
	COMMAND ./mktree env
	DEPENDS tree
)

add_custom_target(shell
	COMMAND ./mktree shell
	DEPENDS tree
)

add_custom_target(check
	COMMAND ./mktree check ${JOBS} $(TESTOPTS)
	DEPENDS tree
)

add_custom_target(reset
	COMMAND ./mktree reset
)

add_custom_command(OUTPUT rpmtests
	COMMAND ${AUTOTEST} -I ${CMAKE_CURRENT_SOURCE_DIR}
		-o rpmtests rpmtests.at
	DEPENDS ${TESTSUITE_AT}
	DEPENDS local.at
)

add_custom_target(tree
	COMMAND ./mktree build
	BYPRODUCTS mktree.output/base
	DEPENDS rpmtests
	DEPENDS ${TESTPROGS}
	DEPENDS ${TOP_TARGETS}
)
