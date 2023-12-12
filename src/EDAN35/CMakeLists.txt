add_executable (EDAN35_Assignment2)

target_sources (
	EDAN35_Assignment2
	PRIVATE
		[[assignment2.hpp]]
		[[assignment2.cpp]]
 "util/Cell.hpp" "util/Cell.cpp" "util/cellularAutomata.hpp" "util/cellularAutomata.cpp" "util/defaultColorPalette.cpp")

target_link_libraries (EDAN35_Assignment2 PRIVATE assignment_setup)

install (TARGETS EDAN35_Assignment2 DESTINATION bin)

copy_dlls (EDAN35_Assignment2 "${CMAKE_CURRENT_BINARY_DIR}")



add_executable (EDAN35_Project
        util/IntersectionTests.cpp
        util/voxel_util.cpp
		util/settings.cpp "util/Cell.cpp" "util/cellularAutomata.cpp" "util/defaultColorPalette.cpp")

target_sources (
	EDAN35_Project
	PRIVATE
		[[project.hpp]]
		[[project.cpp]]
)

target_link_libraries (EDAN35_Project PRIVATE assignment_setup)

install (TARGETS EDAN35_Project DESTINATION bin)

copy_dlls (EDAN35_Project "${CMAKE_CURRENT_BINARY_DIR}")
