add_library (bonobo)
target_sources (
	bonobo
	PUBLIC
		[[Bonobo.h]]
		[[BuildSettings.h]]
		"${CMAKE_BINARY_DIR}/config.hpp"
		[[FPSCamera.h]]
		[[FPSCamera.inl]]
		[[helpers.hpp]]
		[[InputHandler.h]]
		[[Log.h]]
		[[LogView.h]]
		[[node.hpp]]
		[[opengl.hpp]]
		[[ShaderProgramManager.hpp]]
		[[TRSTransform.h]]
		[[TRSTransform.inl]]
		[[various.hpp]]
		[[WindowManager.hpp]]
	PRIVATE
		[[Bonobo.cpp]]
		[[helpers.cpp]]
		[[InputHandler.cpp]]
		[[Log.cpp]]
		[[LogView.cpp]]
		[[node.cpp]]
		[[opengl.cpp]]
		[[ShaderProgramManager.cpp]]
		[[various.cpp]]
		[[WindowManager.cpp]]
)

target_include_directories (
	bonobo
	PUBLIC
		"${CMAKE_SOURCE_DIR}/src"
		"${CMAKE_BINARY_DIR}"
		"${ASSIMP_INCLUDE_DIRS}"
)

target_link_libraries (
	bonobo
	PUBLIC
		${ASSIMP_LIBRARIES}
		external_libs
		glfw
		glm
		$<$<NOT:$<BOOL:${WIN32}>>:dl>
	PRIVATE
		CG_Labs_options
		stb::stb
)

install (TARGETS bonobo DESTINATION lib)
