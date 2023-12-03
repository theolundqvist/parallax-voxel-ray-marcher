

#pragma once
#include "../util/Direction.cpp"
#include "../util/GameObject.cpp"
#include "../util/parametric_shapes.cpp"
#include "../util/parametric_shapes.hpp"
#include "../util/ui.cpp"
#include "VoxelRenderer.cpp"
#include "core/FPSCamera.h"
#include <cstddef>
#include <glm/gtc/type_ptr.hpp>
#include <list>
#include "../util/AppState.cpp"

class App {
public:
  App(FPSCameraf *cam, InputHandler *inputHandler,
      ShaderProgramManager *shaderManager, float *elapsed_time_s) {
    this->renderer = new VoxelRenderer(cam, shaderManager, elapsed_time_s);
    this->inputHandler = inputHandler;
    this->ui = UI();
  }

  void update(bool show_basis, float basis_length_scale,
              float basis_thickness_scale) {
    handleInput();
    switch (state) {
    case RUNNING:
      renderer->render(show_basis, basis_length_scale, basis_thickness_scale);
      if(showCrosshair) ui.crosshair();
      break;
    case PAUSED:
      ui.pauseMenu();
      break;
    }
  }

  void handleInput() {
    if (inputHandler->GetKeycodeState(GLFW_KEY_ESCAPE) & JUST_PRESSED){
			if(state == RUNNING) state = PAUSED;
			else state = RUNNING;
		}

    if(inputHandler->GetKeycodeState(GLFW_KEY_C) & JUST_PRESSED)
      showCrosshair = !showCrosshair;

    if (state == PAUSED &&
        inputHandler->GetKeycodeState(GLFW_KEY_Q) & JUST_PRESSED)
      exit(0);
  }

private:
  VoxelRenderer *renderer;
  bool showCrosshair = false;
  UI ui;
  InputHandler *inputHandler;
};
