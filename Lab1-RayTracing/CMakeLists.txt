cmake_minimum_required (VERSION 3.13)


# Workaround for the lab computers.
if (WIN32)
  set (LUGGCGL_TMP_DIR ${CMAKE_SOURCE_DIR}/../Temp)
  if (NOT EXISTS ${LUGGCGL_TMP_DIR})
    file (MAKE_DIRECTORY ${LUGGCGL_TMP_DIR})
  endif ()
  set (ENV{TMP} ${LUGGCGL_TMP_DIR})
endif ()


# Set project attributes.
project(
  raytracer
  VERSION 2021.0
  DESCRIPTION [[Repository for the first assignment of Computer Graphics course EDAN35 at Lund University, Sweden.]]
  HOMEPAGE_URL [[https://github.com/LUGGPublic/Lab1-RayTracing]]
  LANGUAGES CXX C
)


# Set build type.
set (CMAKE_CONFIGURATION_TYPES [[Debug;Release;RelWithDebInfo]])
if (NOT CMAKE_BUILD_TYPE)
  set (
    CMAKE_BUILD_TYPE
    [[Release]]
    CACHE STRING
    "Choose the type of build, options are: ${CMAKE_CONFIGURATION_TYPES}."
    FORCE
  )
endif (NOT CMAKE_BUILD_TYPE)
set_property (CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${CMAKE_CONFIGURATION_TYPES})

# Set up the executable.
add_executable(raytracer)
target_sources(
  raytracer
  PRIVATE
    [[main.cpp]]
    [[stb_image.h]]
    [[stb_image_write.h]]
    [[swCamera.cpp]]
    [[swCamera.h]]
    [[swCamera.cpp]]
    [[swCamera.h]]
    [[swIntersection.cpp]]
    [[swIntersection.h]]
    [[swMaterial.h]]
    [[swPrimitive.h]]
    [[swRay.h]]
    [[swScene.cpp]]
    [[swScene.h]]
    [[swSphere.cpp]]
    [[swSphere.h]]
    [[swTriangle.cpp]]
    [[swTriangle.h]]
    [[swVec3.h]]
)
target_compile_definitions(
  raytracer
  PRIVATE
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:NOMINMAX>
)
target_compile_features(raytracer PRIVATE cxx_std_11)
target_compile_options(
  raytracer
  PRIVATE
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:/utf-8;/Zc:__cplusplus>
)
