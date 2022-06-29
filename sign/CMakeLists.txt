set(librpmsign_SOURCES
	rpmgensig.c rpmsignverity.c rpmsignfiles.c
)

add_library(librpmsign SHARED)
set_target_properties(librpmsign PROPERTIES
	VERSION ${RPM_LIBVERSION}
	SOVERSION ${RPM_SOVERSION}
)
target_sources(librpmsign PRIVATE ${librpmsign_SOURCES})
target_link_libraries(librpmsign PUBLIC librpmio librpm)
if (WITH_IMA)
	target_link_libraries(librpmsign PRIVATE imaevm)
endif()
if (WITH_FSVERITY)
	target_link_libraries(librpmsign PRIVATE ${FSVERITY_LIBRARIES})
endif()

install(TARGETS librpmsign)
