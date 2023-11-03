#include "WindowManager.hpp"

#include "Log.h"
#include "opengl.hpp"

#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace
{
	const int default_opengl_major_version = 4;
	const int default_opengl_minor_version = 1;
	const int default_glsl_version = default_opengl_major_version * 100 + default_opengl_minor_version * 10;

	void ErrorCallback(int error, char const* description)
	{
		if (error == 65543 || error == 65545)
			LogError("Couldn't create an OpenGL %d.%d context.\nIf you are using old hardware/drivers which support OpenGL 3.3 but not higher, try using the 'OpenGL_3.3' branch.", default_opengl_major_version, default_opengl_minor_version);
		else
			LogError("GLFW error %d was thrown:\n\t%s\n", error, description);
	}

	void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		WindowManager::WindowDatum* const instance = static_cast<WindowManager::WindowDatum*>(glfwGetWindowUserPointer(window));
		instance->input_handler.FeedKeyboard(key, scancode, action);

#ifdef _WIN32
		bool should_close = (key == GLFW_KEY_F4) && (mods == GLFW_MOD_ALT);
#elif defined __APPLE__
		bool should_close = (key == GLFW_KEY_Q) && (mods == GLFW_MOD_SUPER);
#elif defined __linux__
		bool should_close = (key == GLFW_KEY_Q) && (mods == GLFW_MOD_CONTROL);
#else
		bool should_close = false;
#endif
		// should_close |= (key == GLFW_KEY_ESCAPE);
		if (should_close)
			glfwSetWindowShouldClose(window, true);

		ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
	}

	void MouseCallback(GLFWwindow* window, int button, int action, int mods)
	{
		WindowManager::WindowDatum* const instance = static_cast<WindowManager::WindowDatum*>(glfwGetWindowUserPointer(window));
		instance->input_handler.FeedMouseButtons(button, action);
	}

	void CursorCallback(GLFWwindow* window, double x, double y)
	{
		WindowManager::WindowDatum* const instance = static_cast<WindowManager::WindowDatum*>(glfwGetWindowUserPointer(window));
		instance->input_handler.FeedMouseMotion(glm::vec2(x, y));
	}

	void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
	{
		if (width <= 0 || height <= 0)
			return;

		WindowManager::WindowDatum* const instance = static_cast<WindowManager::WindowDatum*>(glfwGetWindowUserPointer(window));
		instance->camera.SetAspect(static_cast<float>(width) / static_cast<float>(height));
	}
} // anonymous namespace

std::mutex WindowManager::mMutex;

WindowManager::WindowManager()
{
	bool const is_first_instance = WindowManager::mMutex.try_lock();
	if (!is_first_instance)
		throw std::runtime_error("[WindowManager] Only one instance of WindowManager can exist at any given point.");

	glfwSetErrorCallback(ErrorCallback);

	int const init_res = glfwInit();
	if (init_res == GLFW_FALSE) {
		WindowManager::mMutex.unlock();
		throw std::runtime_error("[GLFW] Initialisation failure.");
	}
}

WindowManager::~WindowManager()
{
	glfwTerminate();
	WindowManager::mMutex.unlock();
}

GLFWwindow* WindowManager::CreateGLFWWindow(std::string const& title, WindowDatum const& data, unsigned int msaa, bool fullscreen, bool resizable, SwapStrategy swap)
{
#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if DEBUG_LEVEL >= 2
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, default_opengl_major_version);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, default_opengl_minor_version);

	glfwWindowHint(GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);
	glfwWindowHint(GLFW_SAMPLES, static_cast<int>(msaa));

	GLFWmonitor* const monitor = glfwGetPrimaryMonitor();
	GLFWvidmode const* const video_mode = glfwGetVideoMode(monitor);
	int width  = fullscreen ? data.fullscreen_width  : data.windowed_width;
	int height = fullscreen ? data.fullscreen_height : data.windowed_height;
	if (width == 0)
		width = video_mode->width;
	if (height == 0)
		height = video_mode->height;

	glfwWindowHint(GLFW_RED_BITS, video_mode->redBits);
	glfwWindowHint(GLFW_GREEN_BITS, video_mode->greenBits);
	glfwWindowHint(GLFW_BLUE_BITS, video_mode->blueBits);
	glfwWindowHint(GLFW_REFRESH_RATE, video_mode->refreshRate);

	GLFWwindow* window = glfwCreateWindow(width, height, title.c_str(), fullscreen ? monitor : nullptr, nullptr);

	if (window == nullptr)
		return nullptr;

	glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
		LogError("[GLAD]: Failed to initialise OpenGL context.");
		return nullptr;
	}

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	char glsl_version_directive[13];
	std::snprintf(glsl_version_directive, 13, "#version %d%d0", default_opengl_major_version, default_opengl_minor_version);
	ImGui_ImplOpenGL3_Init(glsl_version_directive);

	glfwSetKeyCallback(window, KeyCallback);
	glfwSetInputMode(window, GLFW_STICKY_KEYS, 1);
	glfwSetMouseButtonCallback(window, MouseCallback);
	glfwSetCursorPosCallback(window, CursorCallback);
	glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
	glfwSetWindowUserPointer(window, static_cast<void*>(this));

	glfwSetScrollCallback(window, ImGui_ImplGlfw_ScrollCallback);
	glfwSetCharCallback(window, ImGui_ImplGlfw_CharCallback);

	GLint context_flags = 0, profile_mask = 0;
	glGetIntegerv(GL_CONTEXT_FLAGS, &context_flags);
	glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile_mask);
	LogInfo("Using OpenGL %d.%d with context options: profile=%s, debug=%s, forward compatible=%s.", GLVersion.major, GLVersion.minor
	       , (profile_mask & GL_CONTEXT_CORE_PROFILE_BIT) ? "core" : (profile_mask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) ? "compatibility" : "unknown"
	       , (context_flags & GL_CONTEXT_FLAG_DEBUG_BIT) ? "true" : "false"
	       , (context_flags & GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT) ? "true" : "false"
	       );

	if (utils::opengl::debug::isSupported())
	{
#if DEBUG_LEVEL >= 2
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(utils::opengl::debug::opengl_error_callback, nullptr);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_FALSE);
#endif
#if DEBUG_LEVEL == 2
		// Enable all messages of severity medium or higher.
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_MEDIUM, 0, nullptr, GL_TRUE);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_HIGH, 0, nullptr, GL_TRUE);
#elif DEBUG_LEVEL == 3
		// Enable all messages.
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

		// Comment out the next two calls to get scoped debug messages in the logs.
		// Note that it can come at a significant performance cost.
		glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_PUSH_GROUP, GL_DONT_CARE, 0, nullptr, GL_FALSE);
		glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_POP_GROUP, GL_DONT_CARE, 0, nullptr, GL_FALSE);
#endif
#if DEBUG_LEVEL >= 2
		// Disable certain messages:
		std::array<GLuint, 1> api_other_ids = {
			131185u, // "Buffer detailed info: Buffer object Y will use VIDEO memory as the source for buffer object operations."
		};
		glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE, static_cast<GLsizei>(api_other_ids.size()), api_other_ids.data(), GL_FALSE);
#endif
	}
	else
	{
		LogInfo("DebugCallback is not core in OpenGL %d.%d, and sadly the GL_KHR_DEBUG extension is not available either.", GLVersion.major, GLVersion.minor);
	}

	glfwSwapInterval(static_cast<std::underlying_type<SwapStrategy>::type>(swap));

	auto& datum_copy = mWindowData[window] = std::make_unique<WindowDatum>(data);
	datum_copy->fullscreen_width = width;
	datum_copy->fullscreen_height = height;
	glfwSetWindowUserPointer(window, datum_copy.get());

	return window;
}

void WindowManager::DestroyWindow(GLFWwindow* const window)
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);

	auto const window_datum_iter = mWindowData.find(window);
	if (window_datum_iter != mWindowData.end())
		mWindowData.erase(window_datum_iter);
}

void WindowManager::NewImGuiFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void WindowManager::RenderImGuiFrame(bool show_gui)
{
	ImGui::Render();
	if (show_gui)
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void WindowManager::ToggleFullscreenStatusForWindow(GLFWwindow* const window) noexcept
{
	if (window == nullptr)
		return;

	WindowDatum* const datum = reinterpret_cast<WindowDatum*>(glfwGetWindowUserPointer(window));

	GLFWmonitor* current_monitor = glfwGetWindowMonitor(window);
	if (current_monitor == nullptr) { // We are currentlu windowed.
		// Save the position and size, to reuse if going back windowed
		// later on.
		glfwGetWindowPos(window, &datum->xpos, &datum->ypos);
		glfwGetWindowSize(window, &datum->windowed_width, &datum->windowed_height);

		GLFWmonitor* const monitor = glfwGetPrimaryMonitor();
		GLFWvidmode const* const mode = glfwGetVideoMode(monitor);
		glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
	} else { // We are currently fullscreen.
		glfwSetWindowMonitor(window, nullptr, datum->xpos, datum->ypos, datum->windowed_width, datum->windowed_height, 0);
	}
}
