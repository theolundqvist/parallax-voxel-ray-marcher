cmake_minimum_required (VERSION 3.13)

# Workaround for the lab computers
if (WIN32)
	set (LUGGCGL_TMP_DIR ${CMAKE_SOURCE_DIR}/../Temp)
	if (NOT EXISTS ${LUGGCGL_TMP_DIR})
		file (MAKE_DIRECTORY ${LUGGCGL_TMP_DIR})
	endif ()
	set (ENV{TMP} ${LUGGCGL_TMP_DIR})
endif ()

# Set project attributes.
project (
	CG_Labs
	VERSION 2021.2
	DESCRIPTION [[Repository for Computer Graphics courses EDAF80 and EDAN35 at Lund University, Sweden.]]
	HOMEPAGE_URL [[https://github.com/LUGGPublic/CG_Labs/]]
	LANGUAGES CXX C
)


# Set build type
set (CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo")
if (NOT CMAKE_BUILD_TYPE)
	set (
		CMAKE_BUILD_TYPE
		"Release"
		CACHE STRING
		"Choose the type of build, options are: ${CMAKE_CONFIGURATION_TYPES}."
		FORCE
	)
endif (NOT CMAKE_BUILD_TYPE)
set_property (CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${CMAKE_CONFIGURATION_TYPES})


# To be able to dowload and compile the dependencies
find_package (Git REQUIRED)
include (FetchContent)
set (FETCHCONTENT_BASE_DIR "${PROJECT_SOURCE_DIR}/dependencies" CACHE PATH "Base folder where dependencies are processed." FORCE)

if (NOT EXISTS "${FETCHCONTENT_BASE_DIR}")
	file (MAKE_DIRECTORY "${FETCHCONTENT_BASE_DIR}")
endif ()

# assimp is used for loading the objects’s models
set (LUGGCGL_ASSIMP_MIN_VERSION 5.1.0)
set (LUGGCGL_ASSIMP_DOWNLOAD_VERSION 5.1.2)
include (CMake/InstallAssimp.cmake)
find_package (assimp ${LUGGCGL_ASSIMP_MIN_VERSION} REQUIRED)
link_directories (${ASSIMP_LIBRARY_DIRS})

# GLFW is used for inputs and windows handling
set (LUGGCGL_GLFW_MIN_VERSION 3.2.0)
set (LUGGCGL_GLFW_DOWNLOAD_VERSION 3.3.2)
include (CMake/InstallGLFW.cmake)
find_package (glfw3 ${LUGGCGL_GLFW_MIN_VERSION} REQUIRED)

# GLM is used for matrices, vectors and camera handling
set (LUGGCGL_GLM_DOWNLOAD_VERSION 0.9.9.5)
include (CMake/InstallGLM.cmake)
find_package (glm ${LUGGCGL_GLM_DOWNLOAD_VERSION} EXACT REQUIRED)

# TinyFileDialogs is used for displaying error popups.
include (CMake/InstallTinyFileDialogs.cmake)

# stb is used for loading in image files.
include (CMake/InstallSTB.cmake)

# Resources are found in an external archive
include (CMake/RetrieveResourceArchive.cmake)


# Configure *C++ Environment Variables*
set (MSAA_RATE "1" CACHE STRING "Window MSAA rate")
set (WIDTH "1600" CACHE STRING "Window width")
set (HEIGHT "900" CACHE STRING "Window height")
set (ROOT_DIR "${PROJECT_SOURCE_DIR}")
configure_file ("${PROJECT_SOURCE_DIR}/src/core/config.hpp.in" "${PROJECT_BINARY_DIR}/config.hpp")


# Set up Doxygen documentation generation
option (LUGGCGL_BUILD_DOCUMENTATION "Build documentation for Lund University Computer Graphics Labs" OFF)
if (LUGGCGL_BUILD_DOCUMENTATION)
	find_package (Doxygen REQUIRED)
	set (LUGGCGL_VERSION_NUMBER "${CMAKE_PROJECT_VERSION}")
	set (LUGGCGL_DOC_INPUT "${CMAKE_SOURCE_DIR}/src")
	set (LUGGCGL_DOC_EXCLUDE ${CMAKE_SOURCE_DIR}/src/external)
	set (LUGGCGL_DOC_OUTPUT "${CMAKE_BINARY_DIR}/doc")
	if (NOT EXISTS "${LUGGCGL_DOC_OUTPUT}")
		file (MAKE_DIRECTORY "${LUGGCGL_DOC_OUTPUT}")
	endif ()
	configure_file ("${CMAKE_SOURCE_DIR}/Doxyfile.in" "${CMAKE_BINARY_DIR}/doc/Doxyfile" @ONLY)
	add_custom_target (
		doc
		COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_BINARY_DIR}/doc/Doxyfile"
		WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/doc"
		COMMENT "Generating API documentation with Doxygen"
	)
endif ()


# Define a “fake” library to store the C++ configuration:
# all libraries and executables linking against this target will automatically
# inherit its configuration such as C++ standard version and additional C++
# flags.
add_library (CG_Labs_options INTERFACE)
target_compile_definitions(
	CG_Labs_options
	INTERFACE
		$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:NOMINMAX>
)
target_compile_options (
	CG_Labs_options
	INTERFACE
		$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:/utf-8;/Zc:__cplusplus>
)
target_compile_features (CG_Labs_options INTERFACE cxx_std_20)


# Define another “fake” library that provides a common setup for all
# assignments. At the moment it only contains all common dependencies but it
# could contain more, like specific compilation flags.
add_library (assignment_setup INTERFACE)
target_link_libraries (
       assignment_setup
       INTERFACE
               bonobo
               CG_Labs_options
               tinyfiledialogs::tinyfiledialogs
)


function (copy_dlls target target_folder)
	if (WIN32)
		get_target_property (assimp_dll_path assimp::assimp IMPORTED_LOCATION_RELEASE)
		get_filename_component (assimp_dll "${assimp_dll_path}" NAME)

		if (NOT EXISTS "${target_folder}/${assimp_dll}")
			add_custom_command (
				TARGET ${target}
				POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy ${assimp_dll_path} ${target_folder}
				COMMENT "Copying dependent DLLs"
			)
		endif ()
	endif ()
endfunction ()


add_subdirectory ("${CMAKE_SOURCE_DIR}/src/external")
add_subdirectory ("${CMAKE_SOURCE_DIR}/src/core")
# add_subdirectory ("${CMAKE_SOURCE_DIR}/src/EDAF80")
add_subdirectory ("${CMAKE_SOURCE_DIR}/src/EDAN35")

install (DIRECTORY ${CMAKE_SOURCE_DIR}/shaders DESTINATION bin)
install (DIRECTORY ${CMAKE_SOURCE_DIR}/res DESTINATION bin)
