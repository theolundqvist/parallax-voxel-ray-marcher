

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
        this->camera = cam;
        cam->mWorld.LookAt(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0, 0, 10.f));
        ui = new UI(window);

        // just for showing raycast near and far hit points
        //hitMin = new GameObject("test");
        //hitMax = new GameObject("test");
        //hitMin->setMesh(parametric_shapes::createSphere(0.25f, 10, 10));
        //hitMax->setMesh(parametric_shapes::createSphere(0.25f, 10, 10));
        //GameObject::addShaderToLibrary(shaderManager, "fallback", [](GLuint p) {});
        //hitMin->setShader("fallback");
        //hitMax->setShader("fallback");
    }


    void render(bool show_basis, float basis_length_scale,
                float basis_thickness_scale, float dt) {
        const auto now = std::chrono::high_resolution_clock::now();
        float gpu_time;
        float cpu_time = dt;
        switch (state) {
            case RUNNING:
                gpu_time = renderer->render(show_basis, basis_length_scale, basis_thickness_scale);
                //hitMin->render(camera->GetWorldToClipMatrix(), glm::mat4(1.0f), false, false, false);
                //hitMax->render(camera->GetWorldToClipMatrix(), glm::mat4(1.0f), false, false, false);
                ui->resize();
                if (showFps) ui->fps(gpu_time, cpu_time, 10);
                if (showCrosshair) ui->crosshair();
                break;
            case PAUSED:
                ui->resize();
                ui->pauseMenu();
                break;
        }
        const auto end = std::chrono::high_resolution_clock::now();
        //printf("app render time: %f ms\n", std::chrono::duration<float>(end - now).count()*1000.0f);
    }

    void update(const std::chrono::microseconds deltaTimeUs) {
        const auto now = std::chrono::high_resolution_clock::now();
        handleInput();
        switch (state) {
            case RUNNING:
                glfwSetInputMode(window, GLFW_CURSOR, dragToMove ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
                renderer->update(inputHandler, deltaTimeUs.count() * 0.001f);
                camera->Update(deltaTimeUs, *inputHandler, false, false, dragToMove);
                break;
            case PAUSED:
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                //camera->Update(deltaTimeUs, *inputHandler, false, false, true);
                break;
        }
        const auto end = std::chrono::high_resolution_clock::now();
        //printf("app update time: %f ms\n", std::chrono::duration<float>(end - now).count()*1000.0f);
    }

    void handleInput() {
        if (inputHandler->GetKeycodeState(GLFW_KEY_ESCAPE) & JUST_PRESSED) {
            if (state == RUNNING) state = PAUSED;
            else state = RUNNING;
        }
        if (state == RUNNING) {
            if (inputHandler->GetKeycodeState(GLFW_KEY_C) & JUST_PRESSED) {
                showCrosshair = !showCrosshair;
                dragToMove = !dragToMove;
            }
            if (inputHandler->GetKeycodeState(GLFW_KEY_F) & JUST_PRESSED)
                showFps = !showFps;

            if (inputHandler->GetKeycodeState(GLFW_KEY_SPACE) & JUST_PRESSED ||
            inputHandler->GetKeycodeState(GLFW_MOUSE_BUTTON_1) & JUST_PRESSED) {
                auto hit = renderer->raycast(camera->mWorld.GetTranslation(), camera->mWorld.GetFront());
                if (!hit.miss) {
                    //hitMin->transform.setPos(hit.voxel.world_pos);
                    //hitMax->transform.setPos(hit.voxel.world_pos_far);
                    //hit.volume->setVoxel(hit.index, 0);
                    auto volumes = renderer->sphereIntersect(hit.world_pos, 0.5f);
                    for (auto volume:volumes) {
                        printf("hit %p\n", volume);
                        volume->setSphere(hit.world_pos, 0.5f, 0);
                    }
                }
            }
            if (inputHandler->GetKeycodeState(GLFW_KEY_X) & JUST_PRESSED ||
                inputHandler->GetKeycodeState(GLFW_MOUSE_BUTTON_2) & JUST_PRESSED) {
                // build
            }

        } else if (inputHandler->GetKeycodeState(GLFW_KEY_Q) & JUST_PRESSED)
            exit(0);
    }

private:
    VoxelRenderer *renderer;
    bool showCrosshair = false;
    bool dragToMove = true;
    bool showFps = true;
    UI *ui;
    InputHandler *inputHandler;
    GLFWwindow *window;
    FPSCameraf *camera;

    GameObject *hitMax;
    GameObject *hitMin;
};
