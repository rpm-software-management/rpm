set(librpmbuild_SOURCES 
	build.c files.c misc.c pack.c
	parseSimpleScript.c parseChangelog.c parseDescription.c
	parseFiles.c parsePreamble.c parsePrep.c parseReqs.c parseScript.c
	parseSpec.c parseList.c reqprov.c rpmfc.c spec.c
	parsePolicies.c policies.c
	rpmbuild_internal.h rpmbuild_misc.h
	speclua.c
)

add_library(librpmbuild SHARED)
set_target_properties(librpmbuild PROPERTIES
	VERSION ${RPM_LIBVERSION}
	SOVERSION ${RPM_SOVERSION}
)
target_sources(librpmbuild PRIVATE ${librpmbuild_SOURCES})
target_include_directories(librpmbuild PRIVATE ${LUA_INCLUDE_DIRS})
target_link_libraries(librpmbuild PUBLIC librpmio librpm)
target_link_libraries(librpmbuild PRIVATE
	libmisc
	${LUA_LIBRARIES}
	${POPT_LIBRARIES}
	${MAGIC_LIBRARIES}
	${LIBELF_LIBRARIES}
	${LIBDW_LIBRARIES}
)

if(WITH_CAP)
	target_link_libraries(librpmbuild PRIVATE ${LIBCAP_LIBRARIES})
endif()

if(OpenMP_C_FOUND)
    target_link_libraries(librpmbuild PRIVATE ${OpenMP_C_LIBRARIES})
endif()


install(TARGETS librpmbuild)
