add_library(librpmio SHARED)

set(librpmio_SOURCES
	argv.c base64.c digest.c expression.c macro.c rpmhook.h rpmhook.c
	rpmio.c rpmlog.c rpmmalloc.c rgetopt.c rpmpgp.c rpmpgpval.h
	rpmsq.c rpmsw.c url.c rpmio_internal.h rpmvercmp.c
	rpmver.c rpmstring.c rpmfileutil.c rpmglob.c rpmkeyring.c
	rpmstrpool.c rpmmacro_internal.h rpmlua.c rpmlua.h lposix.c)

if (WITH_INTERNAL_OPENPGP)
	list(APPEND librpmio_SOURCES rpmpgp_internal.h rpmpgp_internal.c)
	if (WITH_OPENSSL)
		find_package(OpenSSL REQUIRED)
		list(APPEND librpmio_SOURCES digest_openssl.c)
		target_include_directories(librpmio PRIVATE
					${OPENSSL_INCLUDE_DIRS})
		target_link_libraries(librpmio PRIVATE ${OPENSSL_LIBRARIES})
	else()
		pkg_check_modules(LIBGCRYPT libgcrypt REQUIRED)
		list(APPEND librpmio_SOURCES digest_libgcrypt.c)
		target_include_directories(librpmio PRIVATE
					${LIBGCRYPT_INCLUDE_DIRS})
		target_link_libraries(librpmio PRIVATE ${LIBGCRYPT_LIBRARIES})
	endif()
else()
	list(APPEND librpmio_SOURCES rpmpgp_sequoia.c)
	target_link_libraries(librpmio PRIVATE ${RPMSEQUOIA_LIBRARIES})
endif()

set_target_properties(librpmio PROPERTIES
	VERSION ${RPM_LIBVERSION}
	SOVERSION ${RPM_SOVERSION}
)
target_sources(librpmio PRIVATE ${librpmio_SOURCES})
target_link_libraries(librpmio PRIVATE ${POPT_LIBRARIES} ${LUA_LIBRARIES})

set(iolibs ZLIB BZIP2 LIBLZMA ZSTD)
foreach(lib ${iolibs})
	if (${${lib}_FOUND})
		target_include_directories(librpmio PRIVATE
					${${lib}_INCLUDE_DIRS})
		target_link_libraries(librpmio PRIVATE ${${lib}_LIBRARIES})
	endif()
endforeach()

install(TARGETS librpmio
)
