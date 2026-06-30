set (VERSION_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/librpmsign.ver)
if (ENABLE_SYMVER)
	set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--version-script=${VERSION_SCRIPT}")
endif()

add_library(librpmsign SHARED)
set_target_properties(librpmsign PROPERTIES
	VERSION ${RPM_LIBVERSION}
	SOVERSION ${RPM_SOVERSION}
	LINK_DEPENDS ${VERSION_SCRIPT}
)
target_sources(librpmsign PRIVATE rpmgensig.cc)

target_include_directories(librpmsign
    PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/rpmio
	${CMAKE_SOURCE_DIR}/lib
	${Intl_INCLUDE_DIRS}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(librpmsign PUBLIC librpmio librpm)
target_link_libraries(librpmsign PRIVATE PkgConfig::POPT ${Intl_LIBRARIES})
if (WITH_IMAEVM)
	target_sources(librpmsign PRIVATE rpmsignfiles.cc)
	target_link_libraries(librpmsign PRIVATE IMA::IMA)
endif()
if (WITH_FSVERITY)
	target_sources(librpmsign PRIVATE rpmsignverity.cc)
	target_link_libraries(librpmsign PRIVATE PkgConfig::FSVERITY)
endif()

install(TARGETS librpmsign EXPORT rpm-targets)
