add_library (external_libs)

target_sources(
	external_libs
	PUBLIC
		[[Dear ImGui/imconfig.h]]
		[[Dear ImGui/imgui.h]]
		[[Dear ImGui/imgui_internal.h]]
		[[Dear ImGui/imgui_impl_glfw.h]]
		[[Dear ImGui/imgui_impl_opengl3.h]]
		[[Dear ImGui/imgui_impl_opengl3_loader.h]]
		[[Dear ImGui/imstb_rectpack.h]]
		[[Dear ImGui/imstb_textedit.h]]
		[[Dear ImGui/imstb_truetype.h]]
		[[GLAD/glad/glad.h]]
		[[GLAD/KHR/khrplatform.h]]
	PRIVATE
		[[Dear ImGui/imgui.cpp]]
		[[Dear ImGui/imgui_demo.cpp]]
		[[Dear ImGui/imgui_draw.cpp]]
		[[Dear ImGui/imgui_tables.cpp]]
		[[Dear ImGui/imgui_widgets.cpp]]
		[[Dear ImGui/imgui_impl_glfw.cpp]]
		[[Dear ImGui/imgui_impl_opengl3.cpp]]
		[[GLAD/glad.c]]
)

target_include_directories (
	external_libs
	SYSTEM
	PUBLIC
		"${CMAKE_SOURCE_DIR}/src/external/Dear ImGui"
		"${CMAKE_SOURCE_DIR}/src/external/GLAD"
)

target_link_libraries (
	external_libs
	PRIVATE
		glfw
		$<$<NOT:$<BOOL:${WIN32}>>:dl>
)

install (TARGETS external_libs DESTINATION lib)
