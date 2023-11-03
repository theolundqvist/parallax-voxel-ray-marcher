
#pragma once

#include "core/InputHandler.h"
#include "core/FPSCamera.h"
#include "core/WindowManager.hpp"


class Window;


namespace edan35
{
	//! \brief Wrapper class for the project
	class Project {
	public:
		//! \brief Default constructor.
		//!
		//! It will initialise various modules of bonobo and retrieve a
		//! window to draw to.
		Project(WindowManager& windowManager);

		//! \brief Default destructor.
		//!
		//! It will release the bonobo modules initialised by the
		//! constructor, as well as the window.
		~Project();

		//! \brief Contains the logic of the assignment, along with the
		//! render loop.
		void run();

	private:
		FPSCameraf     mCamera;
		InputHandler   inputHandler;
		WindowManager& mWindowManager;
		GLFWwindow*    window;
	};
}
