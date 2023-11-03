find_package (tinyfiledialogs QUIET)
if (NOT tinyfiledialogs_FOUND)
	FetchContent_Declare (
		tinyfiledialogs
		GIT_REPOSITORY [[https://git.code.sf.net/p/tinyfiledialogs/code]]
		GIT_TAG [[e11f94cd7887b101d64f74892d769f0139b5e166]]
	)

	FetchContent_GetProperties (tinyfiledialogs)
	if (NOT tinyfiledialogs_POPULATED)
		message (STATUS "Cloning tinyfiledialogs…")
		FetchContent_Populate (tinyfiledialogs)
	endif ()

	add_library( tinyfiledialogs::tinyfiledialogs INTERFACE IMPORTED)
	set_target_properties(tinyfiledialogs::tinyfiledialogs PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${tinyfiledialogs_SOURCE_DIR}"
		INTERFACE_SOURCES "${tinyfiledialogs_SOURCE_DIR}/tinyfiledialogs.c"
	)
endif ()
