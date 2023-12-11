#pragma once

#include "../util/Direction.cpp"
#include "../util/GameObject.cpp"
#include "../util/parametric_shapes.cpp"
#include "../util/parametric_shapes.hpp"
#include "../util/demo_scenes.cpp"
#include "../util/ui.cpp"
#include "VoxelRenderer.cpp"
#include "core/FPSCamera.h"
#include <cstddef>
#include <glm/gtc/type_ptr.hpp>
#include <list>
#include "../util/AppState.cpp"
#include <map>
#include <iostream>

class DemoApp {
private:
    VoxelRenderer *renderer;
    UI *ui;
    InputHandler *inputHandler;
    GLFWwindow *window;
    FPSCameraf *camera;
    ShaderProgramManager *shaderManager;


    typedef struct key_cooldown_t {
        float cooldown;
        float last_press;
    } key_cooldown_t;
    std::map<int, key_cooldown_t> cooldowns = {};
    float *elapsed;
    settings_t settings;
    GameObject *playerBody;

    int current_scene = QUAD_FIXED;
    scene_settings_t *scene = &scenes[current_scene];

public:
    DemoApp(GLFWwindow *window, FPSCameraf *cam, InputHandler *inputHandler,
            ShaderProgramManager *shaderManager, float *elapsed_time_ms) {
        this->window = window;
        this->inputHandler = inputHandler;
        this->camera = cam;
        this->elapsed = elapsed_time_ms;
        this->shaderManager = shaderManager;
        ui = new UI(window);
        this->renderer = new VoxelRenderer(cam, shaderManager, elapsed_time_ms);

        this->playerBody = new GameObject("playerbody");
        playerBody->setMesh(parametric_shapes::createSphere(0.25f, 10, 10));
        GameObject::addShaderToLibrary(shaderManager, "fallback", [](GLuint p) {});
        playerBody->setShader("fallback");
        setScene(0);
    }

    void setScene(int index) {
        current_scene = index;
        scene = &scenes[current_scene];

        // setup camera
        camera->mWorld.SetTranslate(scene->cam.pos);
        camera->mWorld.LookAt(scene->cam.look_at, glm::vec3(0, 1, 0));

        // create volumes and renderer
        auto tf = Transform().translate(glm::vec3(-1.5)).scale(3.0f);
        renderer->remove_volumes();
        //for(int i = 0; i < scene->volumes; i++){
        renderer->add_volume(new VoxelVolume(scene->volume_size, scene->volume_size, scene->volume_size, tf));
        //}
        scene->voxel_count = scene->volume_size * scene->volume_size * scene->volume_size * scene->volumes;
        setupScene();
    }


    void setupScene() {
        int rule = scene->rule;
        float time = *this->elapsed;
        switch (scene->nbr) {
            case SCENES::QUAD_FIXED:
                scene->voxel_count = scene->volume_size * scene->volume_size;
                renderer->getVolume(0)->updateVoxels([rule, time](int x, int y, int z, GLubyte prev) {
                    if (rule == 1) return 255;
                    if (!voxel_util::wave(0.01f, x, y, z, 8)) return rule == 2 ? 1 : 0;
                    return voxel_util::hash(glm::ivec3(x, y, z));
                });
                break;
            case SCENES::CUBE_FIXED:
                renderer->getVolume(0)->updateVoxels([rule, time](int x, int y, int z, GLubyte prev) {
                    if (rule >= 2) {
                        if (!voxel_util::wave(rule == 3 ? time * 0.001f : 1.0f, x, y, z, 8)) return rule == 2 ? 1 : 0;
                    }
                    return voxel_util::hash(glm::ivec3(x, y, z));
                });
                break;
            case SCENES::CUBE_FVTA_STEP:
                renderer->getVolume(0)->updateVoxels([](int x, int y, int z, GLubyte prev) {
                    return voxel_util::hash(glm::ivec3(x, y, z));
                });
                break;
            case SCENES::SHADERS:
                break;
            case SCENES::FREE_VIEW:
                break;
            case SCENES::LARGER:
                break;
            case SCENES::SDF:
                break;
            case SCENES::CA:
                break;
            case SCENES::NOISE:
                break;
            case SCENES::MINECRAFT:
                break;
        }
    }

    typedef struct checkpoint_t {
        float time;
        TRSTransformf cam;
    } checkpoint_t;
    checkpoint_t checkpoint;

    void lookAtBlock(checkpoint_t check, glm::vec3 block_index, glm::vec3 cam_block_index, float time = 1000.0f){
        auto volume = renderer->getVolume(0);
        auto inverse = volume->transform;//.get_inverse();
        auto end_pos = inverse.apply(cam_block_index*volume->voxel_size);
        auto block_pos = inverse.apply(block_index*volume->voxel_size);
        camera->mWorld.SetTranslate(
                voxel_util::lerp(check.cam.GetTranslation(), end_pos,
                                 glm::smoothstep(check.time, check.time + time, *elapsed)));
        camera->mWorld.LookAt(
                voxel_util::lerp(scene->cam.look_at, block_pos,
                                 glm::smoothstep(check.time, check.time + time, *elapsed)), Direction::up);
    }

    void updateScene(const std::chrono::microseconds deltaTimeUs) {
        switch (scene->nbr) {
            case SCENES::QUAD_FIXED:
                if (scene->ruled_changed) {
                    scene->ruled_changed = false;
                    setupScene();
                }
                break;
            case SCENES::CUBE_FIXED:
                if (scene->ruled_changed || scene->rule == 3) {
                    scene->ruled_changed = false;
                    setupScene();
                    if (scene->rule == 4) {
                        checkpoint = {*elapsed, camera->mWorld};
                    }
                    else if(scene->rule == 5){
                        scene->shader_setting = 1; //show fvta
                    }
                    else {
                        scene->shader_setting = 0;
                        camera->mWorld.SetTranslate(scene->cam.pos);
                        camera->mWorld.LookAt(scene->cam.look_at, Direction::up);
                    }
                }
                if (scene->rule >= 4)
                    lookAtBlock(checkpoint, glm::vec3(8, 6, 8), glm::vec3(12, 8, 12));
                scene->orbit = scene->rule == 3;
                break;
            case SCENES::CUBE_FVTA_STEP:
                break;
            case SCENES::SHADERS:
                break;
            case SCENES::FREE_VIEW:
                break;
            case SCENES::LARGER:
                break;
            case SCENES::SDF:
                break;
            case SCENES::CA:
                break;
            case SCENES::NOISE:
                break;
            case SCENES::MINECRAFT:
                break;
        }

        int rule = scene->rule;
        if (getKey(GLFW_KEY_DOWN)) rule -= 1;
        if (getKey(GLFW_KEY_UP)) rule += 1;
        rule = glm::clamp(rule, 1, scene->highest_rule);
        if (rule != scene->rule) {
            scene->rule = rule;
            scene->ruled_changed = true;
        }

        if (getKey(GLFW_KEY_LEFT)) current_scene -= 1;
        if (getKey(GLFW_KEY_RIGHT)) current_scene += 1;
        current_scene = glm::clamp(current_scene, 0, (int) NBR_SCENES - 1);
        if (current_scene != scene->nbr) setScene(current_scene);

        if (scene->orbit) renderer->orbit(deltaTimeUs.count() * 0.001f);

        // dev
        if (getKey(GLFW_KEY_O)) {
            printf("cam pos: %f, %f, %f\n", camera->mWorld.GetTranslation().x, camera->mWorld.GetTranslation().y,
                   camera->mWorld.GetTranslation().z);
        }

    }


    glm::mat4 frozen_view_matrix;

    void render(bool show_basis, float basis_length_scale,
                float basis_thickness_scale, float dt) {
        //printf("cam pos: %f %f %f\n", camera->mWorld.GetTranslation().x, camera->mWorld.GetTranslation().y, camera->mWorld.GetTranslation().z);
        const auto now = std::chrono::high_resolution_clock::now();
        float gpu_time, cpu_time;
        switch (state) {
            case RUNNING:
                if (!scene->free_view) frozen_view_matrix = camera->GetWorldToClipMatrix();
                renderer->setShaderSetting(scene->shader_setting);
                gpu_time = renderer->render(
                        frozen_view_matrix,
                        camera->mWorld.GetTranslation(),
                        show_basis,
                        basis_length_scale,
                        basis_thickness_scale);
                cpu_time = dt - gpu_time;
                if (scene->free_view)
                    playerBody->render(
                            frozen_view_matrix,
                            camera->mWorld.GetMatrix(), true, true, true
                    );
                ui->resize();
                if (settings.show_fps) ui->fps(gpu_time, cpu_time, 10);
                if (settings.show_crosshair) ui->crosshair();
                ui->displaySceneSettings(scene);
                break;
            case PAUSED:
                ui->resize();
                ui->pauseMenu();
                break;
        }
        const auto end = std::chrono::high_resolution_clock::now();
    }

    void update(const std::chrono::microseconds deltaTimeUs) {
        const auto now = std::chrono::high_resolution_clock::now();
        handleInput();
        updateScene(deltaTimeUs);
        switch (state) {
            case RUNNING:
                glfwSetInputMode(window, GLFW_CURSOR,
                                 settings.drag_to_move ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
                //renderer->update(inputHandler, deltaTimeUs.count() * 0.001f);
                camera->Update(deltaTimeUs, *inputHandler, false, false, settings.drag_to_move);
                break;
            case PAUSED:
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                break;
        }
        const auto end = std::chrono::high_resolution_clock::now();
    }

    void handleInput() {
        if (getKey(GLFW_KEY_ESCAPE, JUST_PRESSED)) {
            if (state == RUNNING) state = PAUSED;
            else state = RUNNING;
        }
        if (state == RUNNING) {
            if (getKey(GLFW_KEY_C)) {
                settings.show_crosshair = !settings.show_crosshair;
                settings.drag_to_move = !settings.drag_to_move;
            }
            //if (getKey(GLFW_KEY_F))
            //    settings.show_fps = !settings.show_fps;

            //if (getKey(GLFW_KEY_P)) {
            //    settings.free_view = !settings.free_view;
            //}

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
