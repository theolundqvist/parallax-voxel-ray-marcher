# FMA changes boundary voxels in the versioned world generator.
if (MSVC)
	set_source_files_properties (world/Generate.cpp PROPERTIES COMPILE_OPTIONS "/fp:strict")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	set_source_files_properties (world/Generate.cpp PROPERTIES COMPILE_OPTIONS "-ffp-contract=off")
endif ()

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
		world/Store.cpp
		world/Generate.cpp
		world/BrickPool.cpp
		world/WorldRenderer.cpp
)

target_link_libraries (EDAN35_Project PRIVATE assignment_setup)

install (TARGETS EDAN35_Project DESTINATION bin)

copy_dlls (EDAN35_Project "${CMAKE_CURRENT_BINARY_DIR}")

option (LUGGCGL_BUILD_VOXEL_BENCHMARK "Build the native voxel upload benchmark" OFF)
if (LUGGCGL_BUILD_VOXEL_BENCHMARK)
	add_executable (voxel_benchmark benchmark/voxel_benchmark.cpp util/colorPalette.cpp world/Generate.cpp world/BrickPool.cpp world/WorldRenderer.cpp)
	target_link_libraries (voxel_benchmark PRIVATE assignment_setup)
	if (APPLE)
		target_link_libraries (voxel_benchmark PRIVATE "-framework OpenGL")
	endif ()
	target_compile_definitions (voxel_benchmark PRIVATE
		VOXEL_BENCHMARK_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders/EDAN35")
	add_executable (world_storage_smoke benchmark/world_storage_smoke.cpp world/Store.cpp world/Generate.cpp)
	target_link_libraries (world_storage_smoke PRIVATE assignment_setup)
	add_executable (world_stream_smoke benchmark/world_stream_smoke.cpp world/Store.cpp world/Generate.cpp)
	target_link_libraries (world_stream_smoke PRIVATE assignment_setup)
	add_executable (world_frontier_test benchmark/world_frontier_test.cpp)
	target_link_libraries (world_frontier_test PRIVATE assignment_setup)
	add_executable (world_generate_test benchmark/world_generate_test.cpp world/Generate.cpp)
	target_link_libraries (world_generate_test PRIVATE assignment_setup)
endif ()
