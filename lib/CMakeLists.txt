add_library(librpm SHARED)
set_target_properties(librpm PROPERTIES
	VERSION ${RPM_LIBVERSION}
	SOVERSION ${RPM_SOVERSION}
)

set(librpm_SOURCES
	backend/dbi.c backend/dbi.h backend/dummydb.c
	backend/dbiset.c backend/dbiset.h
	headerutil.c header.c headerfmt.c header_internal.h
	rpmdb.c rpmdb_internal.h
	fprint.c fprint.h tagname.c rpmtd.c
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
	rpmplugins.c rpmplugins.h rpmplugin.h rpmug.c rpmug.h
	rpmtriggers.h rpmtriggers.c rpmvs.c rpmvs.h
)

if(ENABLE_SQLITE)
	list(APPEND librpm_SOURCES backend/sqlite.c)
	target_include_directories(librpm PRIVATE ${SQLITE_INCLUDE_DIRS})
	target_link_libraries(librpm PRIVATE ${SQLITE_LIBRARIES})
endif()

if(ENABLE_NDB)
	list(APPEND librpm_SOURCES
		backend/ndb/glue.c
		backend/ndb/rpmpkg.c backend/ndb/rpmpkg.h
		backend/ndb/rpmidx.c backend/ndb/rpmidx.h
		backend/ndb/rpmxdb.c backend/ndb/rpmxdb.h
	)
endif()

if(ENABLE_BDB_RO)
	list(APPEND librpm_SOURCES backend/bdb_ro.c)
endif()

if(WITH_ACL)
	target_link_libraries(librpm PRIVATE acl)
endif()

if(WITH_CAP)
	target_link_libraries(librpm PRIVATE ${LIBCAP_LIBRARIES})
endif()

add_custom_command(OUTPUT tagtbl.C
	COMMAND AWK=gawk ${CMAKE_CURRENT_SOURCE_DIR}/gentagtbl.sh ${CMAKE_SOURCE_DIR}/include/rpm/rpmtag.h > tagtbl.C
	DEPENDS ${CMAKE_SOURCE_DIR}/include/rpm/rpmtag.h gentagtbl.sh
)

target_sources(librpm PRIVATE ${librpm_SOURCES} tagtbl.C)
target_include_directories(librpm PRIVATE ${LUA_INCLUDE_DIRS})
target_link_libraries(librpm PUBLIC librpmio)
target_link_libraries(librpm PRIVATE ${POPT_LIBRARIES} ${LUA_LIBRARIES})

install(TARGETS librpm)
