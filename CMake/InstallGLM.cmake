find_package (glm EXACT QUIET ${LUGGCGL_GLM_DOWNLOAD_VERSION})
if (NOT glm_FOUND)
	FetchContent_Declare (
		glm
		GIT_REPOSITORY [[https://github.com/g-truc/glm.git]]
		GIT_TAG "${LUGGCGL_GLM_DOWNLOAD_VERSION}"
		GIT_SHALLOW ON

		# GLM does not set its revision version in its number string,
		# making it impossible to match on it.
		PATCH_COMMAND ${GIT_EXECUTABLE} reset --hard HEAD # Remove any existing changes before applying the patch (in case patch is applied twice, for example)
		COMMAND ${GIT_EXECUTABLE} apply ${CMAKE_SOURCE_DIR}/0001-Fix-GLM-version.patch
	)

	FetchContent_GetProperties (glm)
	if (NOT glm_POPULATED)
		message (STATUS "Cloning glm…")
		FetchContent_Populate (glm)
	endif ()

	set (glm_INSTALL_DIR "${FETCHCONTENT_BASE_DIR}/glm-install")
	if (NOT EXISTS "${glm_INSTALL_DIR}")
		file (MAKE_DIRECTORY ${glm_INSTALL_DIR})
	endif ()

	message (STATUS "Setting up CMake for glm…")
	execute_process (
		COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}"
		                         -A "${CMAKE_GENERATOR_PLATFORM}"
		                         -DCMAKE_INSTALL_PREFIX=${glm_INSTALL_DIR}
		                         -DCMAKE_BUILD_TYPE=Release
					 -DGLM_TEST_ENABLE=OFF
		                         ${glm_SOURCE_DIR}
		OUTPUT_VARIABLE stdout
		ERROR_VARIABLE stderr
		RESULT_VARIABLE result
		WORKING_DIRECTORY ${glm_BINARY_DIR}
	)
	if (result)
		message (FATAL_ERROR "CMake setup for glm failed: ${result}\n"
		                     "Standard output: ${stdout}\n"
		                     "Error output: ${stderr}")
	endif ()

	message (STATUS "Building and installing glm…")
	execute_process (
		COMMAND ${CMAKE_COMMAND} --build ${glm_BINARY_DIR}
		                         --config Release
		                         --target install
		OUTPUT_VARIABLE stdout
		ERROR_VARIABLE stderr
		RESULT_VARIABLE result
	)
	if (result)
		message (FATAL_ERROR "Build step for glm failed: ${result}\n"
		                     "Standard output: ${stdout}\n"
		                     "Error output: ${stderr}")
	endif ()

	list (APPEND CMAKE_PREFIX_PATH ${glm_INSTALL_DIR}/lib64/cmake)

	set (glm_INSTALL_DIR)
endif ()
