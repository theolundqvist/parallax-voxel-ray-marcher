#pragma once

#include "FPSCamera.h"
#include "InputHandler.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <mutex>
#include <unordered_map>
#include <memory>

//! \brief A simple class for creating and interacting with windows, using the
//!        GLFW library.
//!
//! Note: Only one instance of this class can be alive at any given time.
//!
//! All windows, which were not manually destroyed using the
//! WindowManager::DestroyWindow method, will be automatically destroyed along
//! with the WindowManager object when it gets deleted.
class WindowManager
{
public:
	enum class SwapStrategy : int {
		disable_vsync = 0,
		enable_vsync = 1,
		late_swap_tearing = -1
	};
	struct WindowDatum {
		InputHandler& input_handler;
		FPSCameraf&   camera;
		int           windowed_width, windowed_height;
		int           fullscreen_width, fullscreen_height;
		int           xpos, ypos;
	};

	WindowManager();
	~WindowManager();

	GLFWwindow* CreateGLFWWindow(std::string const& title, WindowDatum const& data, unsigned int msaa = 1u, bool fullscreen = false, bool resizable = false, SwapStrategy swap = SwapStrategy::enable_vsync);
	void DestroyWindow(GLFWwindow* const window);
	void NewImGuiFrame();
	void RenderImGuiFrame(bool show_gui);
	void ToggleFullscreenStatusForWindow(GLFWwindow* const window) noexcept;

private:
	std::unordered_map<GLFWwindow*, std::unique_ptr<WindowDatum>> mWindowData;

	static std::mutex mMutex;
};
