find_program(PANDOC NAMES pandoc)
mark_as_advanced(PANDOC)

add_subdirectory(man)

if (WITH_DOXYGEN)
    find_package(Doxygen REQUIRED)
endif()
if (DOXYGEN_FOUND)
	# XXX API docs should be pre-built in tarballs
	file(GLOB headers ${CMAKE_SOURCE_DIR}/include/rpm/*.h)
	set(DOXYGEN_WARN_IF_UNDOCUMENTED NO)
	set(DOXYGEN_OPTIMIZE_OUTPUT_FOR_C YES)
	if (ENABLE_WERROR)
		set(DOXYGEN_WARN_AS_ERROR YES)
	endif()
	doxygen_add_docs(apidoc librpm/Doxyheader.h ${headers}
			ALL USE_STAMP_FILE)
	set(doxsrc ${CMAKE_CURRENT_BINARY_DIR})
elseif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/html/index.html)
	set(doxsrc ${CMAKE_CURRENT_SOURCE_DIR})
endif()

if (doxsrc)
	install(DIRECTORY ${doxsrc}/html/ DESTINATION ${CMAKE_INSTALL_DOCDIR}/API)
endif()

install(FILES
	manual/arch_dependencies.md
	manual/autosetup.md
	manual/boolean_dependencies.md
	manual/buildprocess.md
	manual/conditionalbuilds.md
	manual/dependencies.md
	manual/dependency_generators.md
	manual/devel_documentation.md
	manual/dynamic_specs.md
	manual/file_triggers.md
	manual/format_v3.md
	manual/format_v4.md
	manual/index.md
	manual/large_files.md
	manual/lua.md
	manual/macros.md
	manual/more_dependencies.md
	manual/plugins.md
	manual/queryformat.md
	manual/relocatable.md
	manual/scriptlet_expansion.md
	manual/signatures_digests.md
	manual/spec.md
	manual/tags.md
	manual/triggers.md
	manual/tsort.md
	TYPE DOC
)
