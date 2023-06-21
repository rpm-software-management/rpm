# Use our top-level targets as an ordering clue to cmake: the project
# needs to be built before we can populate anything...
get_property(TOP_TARGETS DIRECTORY .. PROPERTY BUILDSYSTEM_TARGETS)

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

configure_file(atlocal.in atlocal @ONLY)
configure_file(package.m4.in package.m4 @ONLY)

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

find_program(FAKECHROOT fakechroot REQUIRED)
find_program(AUTOM4TE autom4te REQUIRED)
set(AUTOTEST ${AUTOM4TE} --language=autotest)

set(TESTPROGS rpmpgpcheck rpmpgppubkeyfingerprint)
foreach(prg ${TESTPROGS})
	add_executable(${prg} EXCLUDE_FROM_ALL ${prg}.c)
	target_link_libraries(${prg} PRIVATE librpmio)
endforeach()

include(ProcessorCount)
ProcessorCount(nproc)
if (nproc GREATER 1)
	set(JOBS -j${nproc})
endif()

add_custom_target(check
	COMMAND ./rpmtests ${JOBS} $(TESTOPTS)
	DEPENDS populate_testing
	DEPENDS rpmtests
	DEPENDS ${TESTPROGS}
)

add_custom_command(OUTPUT rpmtests
	COMMAND ${AUTOTEST} -I ${CMAKE_CURRENT_SOURCE_DIR}
		-o rpmtests rpmtests.at
	DEPENDS ${TESTSUITE_AT}
	DEPENDS local.at
)

add_custom_target(populate_testing
	COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/populate
		${CMAKE_BINARY_DIR}
		${CMAKE_CURRENT_BINARY_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_INSTALL_FULL_BINDIR}
		${CMAKE_MAKE_PROGRAM}
	SOURCES populate
	DEPENDS ${TOP_TARGETS}
)

add_custom_command(TARGET populate_testing POST_BUILD
	COMMAND chmod -R u-w testing
)
add_custom_command(TARGET check POST_BUILD
	COMMAND chmod -R u+w testing
)
