

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
#include <map>
#include <iostream>

class App {
private:
    VoxelRenderer *renderer;
    bool showCrosshair = false;
    bool dragToMove = true;
    bool showFps = true;
    bool freeView = false;
    UI *ui;
    InputHandler *inputHandler;
    GLFWwindow *window;
    FPSCameraf *camera;

    GameObject *hitMax;
    GameObject *hitMin;

    typedef struct key_cooldown_t {
        float cooldown;
        float last_press;
    } key_cooldown_t;
    std::map<int, key_cooldown_t> cooldowns = {};

    float *elapsed;
    settings_t settings;
    GameObject *playerBody;

public:
    App(GLFWwindow *window, FPSCameraf *cam, InputHandler *inputHandler,
        ShaderProgramManager *shaderManager, float *elapsed_time_ms) {
        this->window = window;
        this->renderer = new VoxelRenderer(cam, shaderManager, elapsed_time_ms);
        this->inputHandler = inputHandler;
        this->camera = cam;
        this->elapsed = elapsed_time_ms;
        cam->mWorld.LookAt(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0, 0, 10.f));
        ui = new UI(window);

        this->playerBody = new GameObject("playerbody");
        playerBody->setMesh(parametric_shapes::createSphere(0.25f, 10, 10));
        GameObject::addShaderToLibrary(shaderManager, "fallback", [](GLuint p) {});
        playerBody->setShader("fallback");

        // just for showing raycast near and far hit points
        //hitMin = new GameObject("test");
        //hitMax = new GameObject("test");
        //hitMin->setMesh(parametric_shapes::createSphere(0.25f, 10, 10));
        //hitMax->setMesh(parametric_shapes::createSphere(0.25f, 10, 10));
        //GameObject::addShaderToLibrary(shaderManager, "fallback", [](GLuint p) {});
        //hitMin->setShader("fallback");
        //hitMax->setShader("fallback");
    }


    glm::mat4 frozen_view_matrix;

    void render(bool show_basis, float basis_length_scale,
                float basis_thickness_scale, float dt) {
        const auto now = std::chrono::high_resolution_clock::now();
        float gpu_time, cpu_time;
        switch (state) {
            case RUNNING:
                if (!freeView) frozen_view_matrix = camera->GetWorldToClipMatrix();
                gpu_time = renderer->render(
                        frozen_view_matrix,
                        camera->mWorld.GetTranslation(),
                        show_basis,
                        basis_length_scale,
                        basis_thickness_scale);
                cpu_time = dt - gpu_time;
                if (freeView)
                    playerBody->render(
                            frozen_view_matrix,
                            camera->mWorld.GetMatrix(), true, true, true
                    );
                //hitMin->render(camera->GetWorldToClipMatrix(), glm::mat4(1.0f), false, false, false);
                //hitMax->render(camera->GetWorldToClipMatrix(), glm::mat4(1.0f), false, false, false);
                ui->resize();
                if (showFps) ui->fps(gpu_time, cpu_time, 10);
                if (showCrosshair) ui->crosshair();
                if (showCrosshair) ui->displaySettings(settings);
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
        if (getKey(GLFW_KEY_ESCAPE, JUST_PRESSED)) {
            if (state == RUNNING) state = PAUSED;
            else state = RUNNING;
        }
        if (state == RUNNING) {
            if (getKey(GLFW_KEY_C)) {
                showCrosshair = !showCrosshair;
                dragToMove = !dragToMove;
            }
            if (getKey(GLFW_KEY_F))
                showFps = !showFps;

            if (getKey(GLFW_KEY_P)) {
                freeView = !freeView;
            }

            if (getKey(GLFW_KEY_SPACE, PRESSED, settings.edit_cooldown) ||
                getKey(GLFW_MOUSE_BUTTON_LEFT, PRESSED, settings.edit_cooldown)) {
                auto hit = renderer->raycast(
                        camera->mWorld.GetTranslation(),
                        camera->mWorld.GetFront()
                );
                if (!hit.miss) {
                    auto volumes = renderer->sphereIntersect(hit.world_pos, settings.edit_size);
                    for (auto volume: volumes) {
                        volume->setSphere(hit.world_pos, settings.edit_size, 0);
                    }
                }
            }
            if (getKey(GLFW_KEY_X, PRESSED, settings.edit_cooldown) ||
                getKey(GLFW_MOUSE_BUTTON_RIGHT, PRESSED, settings.edit_cooldown)) {
                auto hit = renderer->raycast(
                        camera->mWorld.GetTranslation(),
                        camera->mWorld.GetFront()
                );
                if (!hit.miss) {
                    auto volumes = renderer->sphereIntersect(hit.world_pos, settings.edit_size);
                    for (auto volume: volumes) {
                        // material = -1, means hash on index
                        volume->setSphere(hit.world_pos, settings.edit_size, -1);
                    }
                }
            }
            if (getKey(GLFW_KEY_DOWN, PRESSED, 300.0f))
                settings.edit_size -= 0.1f;
            if (getKey(GLFW_KEY_UP, PRESSED, 300.0f))
                settings.edit_size += 0.1f;


        } else if (getKey(GLFW_KEY_Q))
            exit(0);
    }


    bool getKey(int code, int key_state = JUST_PRESSED, float cooldown_ms = -1.0f) {
        auto pressed = inputHandler->GetKeycodeState(code) & key_state;
        if (pressed && cooldown_ms > 0.0f) {
            if (cooldowns.count(code) == 0)
                cooldowns.emplace(code, key_cooldown_t{cooldown_ms, *elapsed});
            else {
                auto &cd = cooldowns[code];
                if (*elapsed > cd.last_press + cd.cooldown)
                    cd.last_press = *elapsed;
                else return false;
            }
        }
        return pressed;
    }

};
