find_package (glfw3 QUIET ${LUGGCGL_GLFW_MIN_VERSION})
if (NOT glfw3_FOUND)
	FetchContent_Declare (
		glfw
		GIT_REPOSITORY [[https://github.com/glfw/glfw.git]]
		GIT_TAG "${LUGGCGL_GLFW_DOWNLOAD_VERSION}"
		GIT_SHALLOW ON
	)

	FetchContent_GetProperties (glfw)
	if (NOT glfw_POPULATED)
		message (STATUS "Cloning glfw…")
		FetchContent_Populate (glfw)
	endif ()

	set (glfw_INSTALL_DIR "${FETCHCONTENT_BASE_DIR}/glfw-install")
	if (NOT EXISTS "${glfw_INSTALL_DIR}")
		file (MAKE_DIRECTORY ${glfw_INSTALL_DIR})
	endif ()

	message (STATUS "Setting up CMake for glfw…")
	execute_process (
		COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}"
		                         -A "${CMAKE_GENERATOR_PLATFORM}"
		                         -DGLFW_BUILD_DOCS=OFF
		                         -DGLFW_BUILD_TESTS=OFF
		                         -DGLFW_BUILD_EXAMPLES=OFF
		                         -DCMAKE_INSTALL_PREFIX=${glfw_INSTALL_DIR}
		                         -DCMAKE_BUILD_TYPE=Release
		                         ${glfw_SOURCE_DIR}
		OUTPUT_VARIABLE stdout
		ERROR_VARIABLE stderr
		RESULT_VARIABLE result
		WORKING_DIRECTORY ${glfw_BINARY_DIR}
	)
	if (result)
		message (FATAL_ERROR "CMake setup for glfw failed: ${result}\n"
		                     "Standard output: ${stdout}\n"
		                     "Error output: ${stderr}")
	endif ()

	message (STATUS "Building and installing glfw…")
	execute_process (
		COMMAND ${CMAKE_COMMAND} --build ${glfw_BINARY_DIR}
		                         --config Release
		                         --target install
		OUTPUT_VARIABLE stdout
		ERROR_VARIABLE stderr
		RESULT_VARIABLE result
	)
	if (result)
		message (FATAL_ERROR "Build step for glfw failed: ${result}\n"
		                     "Standard output: ${stdout}\n"
		                     "Error output: ${stderr}")
	endif ()

	list (APPEND CMAKE_PREFIX_PATH ${glfw_INSTALL_DIR}/lib/cmake)

	set (glfw_INSTALL_DIR)
endif ()
