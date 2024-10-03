add_library(librpmbuild SHARED)
set_target_properties(librpmbuild PROPERTIES
	VERSION ${RPM_LIBVERSION}
	SOVERSION ${RPM_SOVERSION}
)
target_sources(librpmbuild PRIVATE
	build.cc files.cc misc.cc pack.cc
	parseSimpleScript.cc parseChangelog.cc parseDescription.cc
	parseFiles.cc parsePreamble.cc parsePrep.cc parseReqs.cc parseScript.cc
	parseSpec.cc parseList.cc reqprov.cc rpmfc.cc spec.cc
	parsePolicies.cc policies.cc
	rpmbuild_internal.hh rpmbuild_misc.hh
	speclua.cc
)

target_include_directories(librpmbuild
    PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/rpmio
	${CMAKE_SOURCE_DIR}/lib
	${Intl_INCLUDE_DIRS}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(librpmbuild PUBLIC librpmio librpm)
target_link_libraries(librpmbuild PRIVATE
	libmisc
	PkgConfig::POPT
	LUA::LUA
	MAGIC::MAGIC
	${Intl_LIBRARIES}
)

if (LIBDW_FOUND)
	target_link_libraries(librpmbuild PRIVATE PkgConfig::LIBDW)
endif()

if (LIBELF_FOUND)
	target_link_libraries(librpmbuild PRIVATE PkgConfig::LIBELF)
endif()

if (Iconv_FOUND)
	target_link_libraries(librpmbuild PRIVATE Iconv::Iconv)
endif()

if(WITH_CAP)
	target_link_libraries(librpmbuild PRIVATE PkgConfig::LIBCAP)
endif()

if(OpenMP_C_FOUND)
	target_link_libraries(librpmbuild PRIVATE OpenMP::OpenMP_C)
endif()

if (OpenMP_C_FOUND)
	target_link_libraries(librpmbuild PRIVATE OpenMP::OpenMP_CXX)
endif()

install(TARGETS librpmbuild EXPORT rpm-targets)
