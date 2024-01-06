# Use our cli tools as an ordering clue to cmake: the project
# needs to be built before we can populate anything. The tools depend
# on all the libraries so we get a good coverage.
get_property(TOOLPROGS DIRECTORY ../tools PROPERTY BUILDSYSTEM_TARGETS)

function(os_release var key)
	execute_process(
		COMMAND sh -c ". /etc/os-release; echo $${key}"
		OUTPUT_STRIP_TRAILING_WHITESPACE
		OUTPUT_VARIABLE value
	)
	string(TOLOWER ${value} value)
	set(${var} ${value} PARENT_SCOPE)
endfunction()

os_release(OS_NAME ID)
os_release(OS_VERSION VERSION_ID)

set(PYTHON ${Python3_EXECUTABLE})
if (WITH_INTERNAL_OPENPGP)
	set(PGP internal)
else()
	set(PGP sequoia)
endif()
set(HAVE_UNSHARE ${HAVE_UNSHARE})

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
	rpmdevel.at
)

set(TESTPROGS rpmpgpcheck rpmpgppubkeyfingerprint)
foreach(prg ${TESTPROGS})
	add_executable(${prg} EXCLUDE_FROM_ALL ${prg}.c)
	target_link_libraries(${prg} PRIVATE librpmio)
endforeach()
string(REPLACE ";" " " TESTPROG_NAMES "${TESTPROGS}")

set(PINNED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/pinned)

include(ProcessorCount)
ProcessorCount(nproc)
if (nproc GREATER 1)
	set(JOBS -j${nproc})
endif()

# Set up mktree
set(MKTREE_BACKEND oci CACHE STRING "Mktree backend to use")
set(DOCKERFILE ${CMAKE_CURRENT_SOURCE_DIR}/Dockerfile.${OS_NAME})
if (MKTREE_BACKEND STREQUAL oci)
	find_program(PODMAN podman)
	find_program(DOCKER docker)
	find_program(FLATPAK_SPAWN flatpak-spawn)
	mark_as_advanced(PODMAN DOCKER FLATPAK_SPAWN)
	set(CONTAINER_ENV /run/.containerenv)

	# Choose most suitable mode
	if (EXISTS ${CONTAINER_ENV} AND FLATPAK_SPAWN)
		set(MKTREE_MODE hybrid)
		set(PODMAN "${FLATPAK_SPAWN} --host podman")
		configure_file(mktree.rootfs mktree.rootfs @ONLY)
	elseif (PODMAN AND EXISTS ${DOCKERFILE})
		set(MKTREE_MODE native)
		configure_file(${DOCKERFILE} Dockerfile COPYONLY)
		add_custom_target(ci
			COMMAND ./mktree.oci build
			COMMAND ./mktree.oci check ${JOBS} $(TESTOPTS)
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		)
	else()
		set(MKTREE_MODE standalone)
		configure_file(Dockerfile Dockerfile COPYONLY)
		find_program(PODMAN NAMES podman docker REQUIRED)
	endif()
elseif (MKTREE_BACKEND STREQUAL rootfs)
	set(MKTREE_MODE native)
	find_program(BWRAP bwrap REQUIRED)
	mark_as_advanced(BWRAP)
endif()
message(STATUS "Using mktree backend: ${MKTREE_BACKEND} "
	       "(mode: ${MKTREE_MODE})")
configure_file(mktree.common mktree.common @ONLY)
configure_file(mktree.${MKTREE_BACKEND} mktree @ONLY)

if (MKTREE_MODE STREQUAL standalone)
	add_custom_target(tree COMMAND ./mktree build)
else()
	find_program(AUTOM4TE autom4te REQUIRED)
	mark_as_advanced(AUTOM4TE)
	set(AUTOTEST ${AUTOM4TE} --language=autotest)

	configure_file(atlocal.in atlocal @ONLY)
	configure_file(package.m4.in package.m4 @ONLY)

	add_custom_command(OUTPUT rpmtests
		COMMAND ${AUTOTEST} -I ${CMAKE_CURRENT_SOURCE_DIR}
			-o rpmtests rpmtests.at
		DEPENDS ${TESTSUITE_AT}
		DEPENDS local.at
	)

	add_custom_target(tree
		COMMAND ./mktree build
		DEPENDS rpmtests
		DEPENDS ${TESTPROGS}
		DEPENDS ${TOOLPROGS}
	)
endif()

add_custom_target(atshell
	COMMAND ./mktree atshell ||:
	DEPENDS tree
)

add_custom_target(pinned
	COMMAND ./mktree atshell /pinned/update
	COMMAND cp pinned/*.txt ${PINNED_DIR}/
	COMMAND git add ${PINNED_DIR}/*.txt
	COMMAND git diff --staged ${PINNED_DIR}/*.txt
	DEPENDS tree
)

add_custom_target(shell
	COMMAND ./mktree shell ||:
	DEPENDS tree
)

add_custom_target(check
	COMMAND ./mktree check ${JOBS} $(TESTOPTS)
	DEPENDS tree
)

add_custom_target(reset
	COMMAND ./mktree reset
)
