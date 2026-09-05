add_executable (EDAN35_Assignment2)

target_sources (
	EDAN35_Assignment2
	PRIVATE
		[[assignment2.hpp]]
		[[assignment2.cpp]]
 "util/Cell.hpp" "util/Cell.cpp" "util/cellularAutomata.hpp" "util/cellularAutomata.cpp" "util/colorPalette.hpp" "util/noise.cpp" "util/terrain.cpp" "util/colorPalette.cpp")

target_link_libraries (EDAN35_Assignment2 PRIVATE assignment_setup)

install (TARGETS EDAN35_Assignment2 DESTINATION bin)

copy_dlls (EDAN35_Assignment2 "${CMAKE_CURRENT_BINARY_DIR}")



add_executable (EDAN35_Project
        util/IntersectionTests.cpp
        util/voxel_util.cpp
		util/settings.cpp
        util/terrain.cpp "util/Cell.cpp" "util/cellularAutomata.cpp"  "util/noise.cpp"  "util/terrain.cpp" "util/colorPalette.cpp")

target_sources (
	EDAN35_Project
	PRIVATE
		[[project.hpp]]
		[[project.cpp]]
)

target_link_libraries (EDAN35_Project PRIVATE assignment_setup)

install (TARGETS EDAN35_Project DESTINATION bin)

copy_dlls (EDAN35_Project "${CMAKE_CURRENT_BINARY_DIR}")

option (LUGGCGL_BUILD_VOXEL_BENCHMARK "Build the native voxel upload benchmark" OFF)
if (LUGGCGL_BUILD_VOXEL_BENCHMARK)
	add_executable (voxel_benchmark benchmark/voxel_benchmark.cpp util/colorPalette.cpp)
	target_link_libraries (voxel_benchmark PRIVATE assignment_setup)
	if (APPLE)
		target_link_libraries (voxel_benchmark PRIVATE "-framework OpenGL")
	endif ()
	target_compile_definitions (voxel_benchmark PRIVATE
		VOXEL_BENCHMARK_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders/EDAN35")
endif ()
