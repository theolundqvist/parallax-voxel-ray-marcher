

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
    App(GLFWwindow *window, FPSCameraf *cam, InputHandler *inputHandler,
        ShaderProgramManager *shaderManager, float *elapsed_time_s) {
        this->window = window;
        this->renderer = new VoxelRenderer(cam, shaderManager, elapsed_time_s);
        this->inputHandler = inputHandler;
        this->ui = UI();
        this->camera = cam;
        cam->mWorld.LookAt(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0, 0, -10.f));
    }

    void render(bool show_basis, float basis_length_scale,
                float basis_thickness_scale) {
        switch (state) {
            case RUNNING:
                renderer->render(show_basis, basis_length_scale, basis_thickness_scale);
                if (showCrosshair) ui.crosshair();
                break;
            case PAUSED:
                ui.pauseMenu();
                break;
        }
    }

    void update(const std::chrono::microseconds deltaTimeUs) {
        handleInput();
        switch (state) {
            case RUNNING:
                glfwSetInputMode(window, GLFW_CURSOR, dragToMove ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
                camera->Update(deltaTimeUs, *inputHandler, false, false, dragToMove);
                break;
            case PAUSED:
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                //camera->Update(deltaTimeUs, *inputHandler, false, false, true);
                break;
        }
    }

    void handleInput() {
        if (inputHandler->GetKeycodeState(GLFW_KEY_ESCAPE) & JUST_PRESSED) {
            if (state == RUNNING) state = PAUSED;
            else state = RUNNING;
        }

        if (inputHandler->GetKeycodeState(GLFW_KEY_C) & JUST_PRESSED) {
            dragToMove = !dragToMove;
            showCrosshair = !showCrosshair;
        }

        if (state == PAUSED &&
            inputHandler->GetKeycodeState(GLFW_KEY_Q) & JUST_PRESSED)
            exit(0);
    }

private:
    VoxelRenderer *renderer;
    bool showCrosshair = false;
    bool dragToMove = true;
    UI ui;
    InputHandler *inputHandler;
    GLFWwindow *window;
    FPSCameraf *camera;
};
