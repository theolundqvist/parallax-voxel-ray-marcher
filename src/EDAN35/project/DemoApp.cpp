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

    int current_scene = SCENES::QUAD;
    scene_settings_t *scene = &scenes[current_scene];

    typedef struct checkpoint_t {
        float time;
        TRSTransformf cam;
    } checkpoint_t;
    checkpoint_t checkpoint;

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
        playerBody->setMesh(parametric_shapes::createSphere(.25f, 10, 10));
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
        scene->rule = 1;
        scene->ruled_changed = true;
    }


    void updateScene(const std::chrono::microseconds deltaTimeUs) {
        settings.free_view = false;
        int rule = scene->rule;
        float time = *elapsed;
        switch (scene->nbr) {
            // red/hashed texels/wave texels, rest black/rest transparent
            case SCENES::QUAD:
                if (scene->ruled_changed) {
                    scene->voxel_count = scene->volume_size * scene->volume_size;
                    renderer->getVolume(0)->updateVoxels([rule, time](int x, int y, int z, GLubyte prev) {
                        if (rule == 1) return 255;
                        if (rule > 2 && !voxel_util::wave(0.01f, x, y, z, 8)) return rule == 3 ? 1 : 0;
                        return voxel_util::hash(glm::ivec3(x, y, z));
                    });
                }
                break;
            // red/hashed texels/free_view/wave texels, rest black/rest transparent
            case SCENES::CUBE:
                if (scene->ruled_changed || scene->rule == 5) {
                    renderer->getVolume(0)->updateVoxels([rule, time](int x, int y, int z, GLubyte prev) {
                        if(rule == 1) return 255;
                        if (rule >= 4) {
                            if (!voxel_util::wave(rule == 5 ? time * 0.001f : 1.0f, x, y, z, 8)) return rule == 4 ? 1 : 0;
                        }
                        return voxel_util::hash(glm::ivec3(x, y, z));
                    });
                    if(scene->rule == 3){
                        checkpoint = {*elapsed, camera->mWorld};
                    }
                    else {
                        camera->mWorld.SetTranslate(scene->cam.pos);
                        camera->mWorld.LookAt(scene->cam.look_at, Direction::up);
                    }
                }
                scene->orbit = scene->rule == 5;
                settings.free_view = scene->rule == 3;
                if(scene->rule == 3){
                    camera->mWorld.SetTranslate(checkpoint.cam.GetTranslation() * 2.0f);
                    playerBody->transform.setPos(checkpoint.cam.GetTranslation()*1.04f - glm::vec3(0,0.1,0));
                    playerBody->transform.rotateAroundY(0.0005f * (checkpoint.time-time));
                    playerBody->transform.lookAt(scene->cam.look_at, Direction::up);
                }
                break;
            // wave/zoom in/switch to FVTA/show different shaders
            case SCENES::FVTA:
                if(scene->ruled_changed) {
                    renderer->getVolume(0)->updateVoxels([](int x, int y, int z, GLubyte prev) {
                        if (!voxel_util::wave(1.0f, x, y, z, 8)) return 0;
                        return voxel_util::hash(glm::ivec3(x, y, z));
                    });
                    if (scene->rule == 1) {
                        checkpoint = {*elapsed, camera->mWorld};
                    }
                }
                if (scene->rule == 3) {
                    scene->shader_setting = 1; //show fvta
                } else {
                    scene->shader_setting = 0;
                    camera->mWorld.SetTranslate(scene->cam.pos);
                    camera->mWorld.LookAt(scene->cam.look_at, Direction::up);
                }
                if (scene->rule > 1)
                    lookAtBlock(checkpoint, glm::vec3(8, 6, 8), glm::vec3(12, 8, 12));
                break;
            case SCENES::SHADERS:
                if(scene->ruled_changed) {
                    renderer->getVolume(0)->updateVoxels([](int x, int y, int z, GLubyte prev) {
                        if (!voxel_util::wave(1.0f, x, y, z, 8)) return 0;
                        return voxel_util::hash(glm::ivec3(x, y, z));
                    });
                }
                break;
            case SCENES::LARGER:
                if(scene->ruled_changed) {
                    renderer->getVolume(0)->updateVoxels([](int x, int y, int z, GLubyte prev) {
                        if (!voxel_util::wave(1.0f, x, y, z, 64)) return 0;
                        return voxel_util::hash(glm::ivec3(x, y, z));
                    });
                }
                break;
            case SCENES::SDF:
                if(scene->ruled_changed) {
                    renderer->getVolume(0)->updateVoxels([](int x, int y, int z, GLubyte prev) {
                        // ??
                        return 1;
                    });
                }
                break;
            case SCENES::CA:
                if(scene->ruled_changed) {
                    renderer->getVolume(0)->updateVoxels([](int x, int y, int z, GLubyte prev) {
                        // reset generation
                        return 1;
                    });
                }
                renderer->getVolume(0)->updateVoxels([](int x, int y, int z, GLubyte prev) {
                    // next generation
                    return 1;
                });
                break;
            case SCENES::NOISE:
                if(scene->ruled_changed) {
                    renderer->getVolume(0)->updateVoxels([](int x, int y, int z, GLubyte prev) {
                        // ??
                        return 1;
                    });
                }
                break;
            case SCENES::MINECRAFT:
                if(scene->ruled_changed) {
                    // create volumes
                    // figure out which volumes to add/remove
                    if(rule == renderer->numberVolumes()) break;
                    while(rule < renderer->numberVolumes())
                        // remove volume
                        renderer->removeVolume(renderer->numberVolumes()-1);

                    auto center = Transform().translate(glm::vec3(-1.5)).scale(3.0f);
                    int size = scene->volume_size;
                    while(rule > renderer->numberVolumes()){
                        //add volume
                        int n = renderer->numberVolumes() + 1;
                        auto vol = new VoxelVolume(size, size, size, center.translatedX(n * 3).translateZ(n * 3));
                        vol->updateVoxels([](int x, int y, int z, GLubyte prev) {
                            return voxel_util::hash(glm::ivec3(x, y, z));
                        });
                        renderer->add_volume(vol);
                    }
                }
                break;
        }

        if(scene->ruled_changed) scene->ruled_changed = false;
        if (getKey(GLFW_KEY_DOWN)) rule -= 1;
        if (getKey(GLFW_KEY_UP)) rule += 1;

        int intValue_demo = static_cast<int>(S_mananger);
        if (getKey(GLFW_KEY_N)) {
            intValue_demo++;
        }
        if (intValue_demo > 9)
        {
            intValue_demo = 0;
        }
        S_mananger = static_cast<shader_setting>(intValue_demo);

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



    void render(bool show_basis, float basis_length_scale,
                float basis_thickness_scale, float dt) {
        //printf("cam pos: %f %f %f\n", camera->mWorld.GetTranslation().x, camera->mWorld.GetTranslation().y, camera->mWorld.GetTranslation().z);
        const auto now = std::chrono::high_resolution_clock::now();
        float gpu_time, cpu_time;
        switch (state) {
            case RUNNING:
                renderer->setShaderSetting(scene->shader_setting);
                gpu_time = renderer->render(
                        camera->GetWorldToClipMatrix(),
                        settings.free_view ? playerBody->transform.getPos() : camera->mWorld.GetTranslation(),
                        show_basis,
                        basis_length_scale,
                        basis_thickness_scale);
                cpu_time = dt - gpu_time;
                if (settings.free_view)
                    playerBody->render(
                            camera->GetWorldToClipMatrix(),
                            glm::mat4(1.0), true, basis_length_scale,basis_thickness_scale
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

    void lookAtBlock(checkpoint_t check, glm::vec3 block_index, glm::vec3 cam_block_index, float time = 1000.0f) {
        auto volume = renderer->getVolume(0);
        auto inverse = volume->transform;//.get_inverse();
        auto end_pos = inverse.apply(cam_block_index * volume->voxel_size);
        auto block_pos = inverse.apply(block_index * volume->voxel_size);
        camera->mWorld.SetTranslate(
                voxel_util::lerp(check.cam.GetTranslation(), end_pos,
                                 glm::smoothstep(check.time, check.time + time, *elapsed)));
        camera->mWorld.LookAt(
                voxel_util::lerp(scene->cam.look_at, block_pos,
                                 glm::smoothstep(check.time, check.time + time, *elapsed)), Direction::up);
    }

};
