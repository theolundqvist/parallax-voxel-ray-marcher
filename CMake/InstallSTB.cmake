find_package (Stb QUIET)
if (NOT Stb_FOUND)
	FetchContent_Declare (
		stb
		GIT_REPOSITORY [[https://github.com/nothings/stb]]
		GIT_TAG [[c0c982601f40183e74d84a61237e968dca08380e]]
	)

	FetchContent_GetProperties (stb)
	if (NOT stb_POPULATED)
		message (STATUS "Cloning stb…")
		FetchContent_Populate (stb)
	endif ()

	add_library (stb::stb INTERFACE IMPORTED)
	set_target_properties (stb::stb PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${stb_SOURCE_DIR}"
		INTERFACE_SOURCES "${CMAKE_SOURCE_DIR}/src/core/stb_impl.c"
	)
else ()
	# vcpkg has its own FindStb.cmake which defines `Stb_FOUND` and
	# `Stb_INCLUDE_DIR`, but not library which can be readily linked
	# against. So wrap an interface library around those sources, similar
	# to when we retrieve the library sources ourselves.
	add_library (stb::stb INTERFACE IMPORTED)
	set_target_properties (stb::stb PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${Stb_INCLUDE_DIR}"
		INTERFACE_SOURCES "${CMAKE_SOURCE_DIR}/src/core/stb_impl.c"
	)
endif ()
