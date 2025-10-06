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

target_include_directories(librpm
    PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/rpmio
	${CMAKE_CURRENT_BINARY_DIR}
	${Intl_INCLUDE_DIRS}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        # This is needed for rpmcli.hh
        ${POPT_INCLUDE_DIRS}
)

target_sources(librpm PRIVATE
	backend/dbi.cc backend/dbi.hh backend/dummydb.cc
	backend/dbiset.cc backend/dbiset.hh
	headerutil.cc header.cc headerfmt.cc header_internal.hh
	keystore.cc keystore.hh
	rpmdb.cc rpmdb_internal.hh
	fprint.cc fprint.hh tagname.cc rpmtd.cc tagtbl.inc
	cpio.cc cpio.hh depends.cc order.cc formats.cc tagexts.cc fsm.cc fsm.hh
	manifest.cc manifest.hh package.cc
	poptALL.cc poptI.cc poptQV.cc psm.cc query.cc
	rpmal.cc rpmal.hh rpmchecksig.cc rpmds.cc rpmds_internal.hh
	rpmfi.cc rpmfi_internal.hh
	rpmgi.hh rpmgi.cc rpminstall.cc rpmts_internal.hh
	rpmlead.cc rpmlead.hh rpmps.cc rpmprob.cc rpmrc.cc
	rpmte.cc rpmte_internal.hh rpmts.cc rpmfs.hh rpmfs.cc
	signature.cc signature.hh transaction.cc
	verify.cc rpmlock.cc rpmlock.hh misc.hh relocation.cc
	rpmscript.hh rpmscript.cc
	rpmchroot.cc rpmchroot.hh
	rpmplugins.cc rpmplugins.hh rpmug.cc rpmug.hh
	rpmtriggers.hh rpmtriggers.cc rpmvs.cc rpmvs.hh
)

if(ENABLE_SQLITE)
	target_sources(librpm PRIVATE backend/sqlite.cc)
	target_link_libraries(librpm PRIVATE PkgConfig::SQLITE)
endif()

if(ENABLE_NDB)
	target_sources(librpm PRIVATE
		backend/ndb/glue.cc
		backend/ndb/rpmpkg.c backend/ndb/rpmpkg.h
		backend/ndb/rpmidx.c backend/ndb/rpmidx.h
		backend/ndb/rpmxdb.c backend/ndb/rpmxdb.h
	)
endif()

if(ENABLE_BDB_RO)
	target_sources(librpm PRIVATE backend/bdb_ro.cc)
endif()

if(WITH_ACL)
	target_link_libraries(librpm PRIVATE PkgConfig::LIBACL)
endif()

if(WITH_CAP)
	target_link_libraries(librpm PRIVATE PkgConfig::LIBCAP)
endif()

add_custom_command(OUTPUT tagtbl.inc
	COMMAND AWK=${AWK} ${CMAKE_CURRENT_SOURCE_DIR}/gentagtbl.sh ${CMAKE_SOURCE_DIR}/include/rpm/rpmtag.h > tagtbl.inc
	DEPENDS ${CMAKE_SOURCE_DIR}/include/rpm/rpmtag.h gentagtbl.sh
)


target_link_libraries(librpm PUBLIC librpmio)
target_link_libraries(librpm PRIVATE PkgConfig::POPT LUA::LUA ${Intl_LIBRARIES})
target_compile_options(librpm PRIVATE -Wno-sign-compare)

install(TARGETS librpm EXPORT rpm-targets)
