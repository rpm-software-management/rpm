add_library(librpm SHARED)
set_target_properties(librpm PROPERTIES
	VERSION ${RPM_LIBVERSION}
	SOVERSION ${RPM_SOVERSION}
)

target_compile_definitions(librpm PRIVATE
	LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
	LOCALSTATEDIR="${CMAKE_INSTALL_FULL_LOCALSTATEDIR}"
	SYSCONFDIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}"
	LIBRPMALIAS_FILENAME="rpmpopt-${PROJECT_VERSION}"
	LIBRPMALIAS_EXECPATH="${CMAKE_INSTALL_FULL_BINDIR}"
)

target_include_directories(librpm PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/rpmio
	${CMAKE_CURRENT_BINARY_DIR}
)

target_sources(librpm PRIVATE
	backend/dbi.c backend/dbi.h backend/dummydb.c
	backend/dbiset.c backend/dbiset.h
	headerutil.c header.c headerfmt.c header_internal.h
	rpmdb.c rpmdb_internal.h
	fprint.c fprint.h tagname.c rpmtd.c tagtbl.C
	cpio.c cpio.h depends.c order.c formats.c tagexts.c fsm.c fsm.h
	manifest.c manifest.h package.c
	poptALL.c poptI.c poptQV.c psm.c query.c
	rpmal.c rpmal.h rpmchecksig.c rpmds.c rpmds_internal.h
	rpmfi.c rpmfi_internal.h
	rpmgi.h rpmgi.c rpminstall.c rpmts_internal.h
	rpmlead.c rpmlead.h rpmps.c rpmprob.c rpmrc.c
	rpmte.c rpmte_internal.h rpmts.c rpmfs.h rpmfs.c
	signature.c signature.h transaction.c
	verify.c rpmlock.c rpmlock.h misc.h relocation.c
	rpmscript.h rpmscript.c
	rpmchroot.c rpmchroot.h
	rpmplugins.c rpmplugins.h rpmug.c rpmug.h
	rpmtriggers.h rpmtriggers.c rpmvs.c rpmvs.h
)

if(ENABLE_SQLITE)
	target_sources(librpm PRIVATE backend/sqlite.c)
	target_link_libraries(librpm PRIVATE PkgConfig::SQLITE)
endif()

if(ENABLE_NDB)
	target_sources(librpm PRIVATE
		backend/ndb/glue.c
		backend/ndb/rpmpkg.c backend/ndb/rpmpkg.h
		backend/ndb/rpmidx.c backend/ndb/rpmidx.h
		backend/ndb/rpmxdb.c backend/ndb/rpmxdb.h
	)
endif()

if(ENABLE_BDB_RO)
	target_sources(librpm PRIVATE backend/bdb_ro.c)
endif()

if(WITH_ACL)
	target_link_libraries(librpm PRIVATE PkgConfig::LIBACL)
endif()

if(WITH_CAP)
	target_link_libraries(librpm PRIVATE PkgConfig::LIBCAP)
endif()

add_custom_command(OUTPUT tagtbl.C
	COMMAND AWK=gawk ${CMAKE_CURRENT_SOURCE_DIR}/gentagtbl.sh ${CMAKE_SOURCE_DIR}/include/rpm/rpmtag.h > tagtbl.C
	DEPENDS ${CMAKE_SOURCE_DIR}/include/rpm/rpmtag.h gentagtbl.sh
)

target_link_libraries(librpm PUBLIC librpmio)
target_link_libraries(librpm PRIVATE PkgConfig::POPT LUA::LUA)

install(TARGETS librpm EXPORT rpm-targets)
