

#pragma once
#include "../util/Direction.cpp"
#include "../util/GameObject.cpp"
#include "../util/parametric_shapes.cpp"
#include "../util/parametric_shapes.hpp"
#include "../util/ui.cpp"
#include "Renderer.cpp"
#include "core/FPSCamera.h"
#include <cstddef>
#include <glm/gtc/type_ptr.hpp>
#include <list>

class App {
public:
  enum State { PAUSED, RUNNING };
  State state = State::RUNNING;

  App(FPSCameraf *cam, InputHandler *inputHandler,
      ShaderProgramManager *shaderManager, float *elapsed_time_s) {
    this->renderer = new Renderer(cam, shaderManager, elapsed_time_s);
    this->inputHandler = inputHandler;
    this->ui = UI();
  }

  void update(bool show_basis, float basis_length_scale,
              float basis_thickness_scale) {
    handleInput();
    switch (state) {
    case RUNNING:
      renderer->render(show_basis, basis_length_scale, basis_thickness_scale);
      break;
    case PAUSED:
			auto state = this->state;
      ui.pauseMenu([&state]() -> void { state = RUNNING; });
      break;
    }
  }

  void handleInput() {
    if (inputHandler->GetKeycodeState(GLFW_KEY_ESCAPE) & JUST_PRESSED){
			if(state == RUNNING) state = PAUSED;
			else state = RUNNING;
		}

    if (state == PAUSED &&
        inputHandler->GetKeycodeState(GLFW_KEY_Q) & JUST_PRESSED)
      exit(0);
  }

private:
  Renderer *renderer;
  UI ui;
  InputHandler *inputHandler;
};
