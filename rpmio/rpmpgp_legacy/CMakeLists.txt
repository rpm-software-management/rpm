# At least for now, this is can't be built as a standalone project. Ie,
# it can only be built as a part of rpm build tree.
add_library(rpmpgp_legacy OBJECT)

target_sources(rpmpgp_legacy PRIVATE
	rpmpgp_internal.h rpmpgp_internal.c
)
if (WITH_OPENSSL)
	target_sources(rpmpgp_legacy PRIVATE digest_openssl.c)
else()
	target_sources(rpmpgp_legacy PRIVATE digest_libgcrypt.c)
endif()
target_include_directories(rpmpgp_legacy PRIVATE ..)
target_include_directories(rpmpgp_legacy PRIVATE ${Intl_INCLUDE_DIRS})

