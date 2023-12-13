#pragma once

#include "../util/Direction.cpp"
#include "../util/GameObject.cpp"
#include "../util/sdf.cpp"
#include "../util/parametric_shapes.cpp"
#include "../util/parametric_shapes.hpp"
#include "../util/demo_scenes.cpp"
#include "../util/ui.cpp"
#include "../util/terrain_generator.cpp"
#include "VoxelRenderer.cpp"
#include "core/FPSCamera.h"
#include <cstddef>
#include <glm/gtc/type_ptr.hpp>
#include <list>
#include "../util/AppState.cpp"
#include <map>
#include <iostream>

#include "../util/cellularAutomata.hpp"

#include "../util/terrain.hpp"

class DemoApp {
private:
    VoxelRenderer *renderer;
    UI *ui;
    InputHandler *inputHandler;
    GLFWwindow *window;
    FPSCameraf *camera;
    ShaderProgramManager *shaderManager;

    // for ca test
    cellularAutomata *ca3d;
    terrain *t;


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
        glm::vec2 terrain_offset;
    } checkpoint_t;
    checkpoint_t checkpoint;

    TerrainGenerator *terrainGenerator;

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
        setScene(7);

        terrainGenerator = new TerrainGenerator(128, 128, 128, 100);
        terrainGenerator->start();
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
        if (scene->nbr != MINECRAFT) {
            renderer->add_volume(new VoxelVolume(scene->volume_size, scene->volume_size, scene->volume_size, tf));
            scene->rule = 1;
            scene->lod = true;
        }
        //}

        // for ca test
        if (scene->nbr == CA) {
            this->ca3d = new cellularAutomata(cellularAutomata::CARules[0].state, glm::vec3(scene->volume_size),
                                              glm::vec3(scene->volume_size), colorPalette::CAColorsBlue2Pink,
                                              cellularAutomata::drawModes(2));
            // set the color palette
            //this->renderer->getVolume(0)->generateColorPalette(ca3d->colorPalette, glm::vec2(0, 255));
        }

        // for noise test
        /*if (scene->nbr == NOISE) {
            glm::vec3 volumeSize = glm::vec3(scene->volume_size);
            this->t = new terrain(volumeSize.x * 2, volumeSize.z * 2, 50.0f, 4, 0.5f, 2.0f, 0);
        }*/

        scene->voxel_count = scene->volume_size * scene->volume_size * scene->volume_size * scene->volumes;
        scene->ruled_changed = true;
    }


    void updateScene(const std::chrono::microseconds deltaTimeUs) {
        auto size = glm::vec3(scene->volume_size);
        auto volume0 = renderer->getVolume(0);
        settings.free_view = false;
        int rule = scene->rule;
        float time = *elapsed;
        switch (scene->nbr) {
            // red/hashed texels/wave texels, rest black/rest transparent
            case SCENES::QUAD:
                if (scene->ruled_changed) {
                    scene->voxel_count = scene->volume_size * scene->volume_size;
                    volume0->updateVoxels([rule, time](int x, int y, int z, GLubyte prev) {
                        if (rule == 1) return 255;
                        if (rule > 2 && !voxel_util::wave(0.01f, x, y, z, 8)) return rule == 3 ? 1 : 0;
                        return voxel_util::hash(glm::ivec3(x, y, z));
                    });
                }
                break;
                // red/hashed texels/free_view/wave texels, rest black/rest transparent
            case SCENES::CUBE:
                if (scene->ruled_changed || scene->rule == 5) {
                    volume0->updateVoxels([rule, time](int x, int y, int z, GLubyte prev) {
                        if (rule == 1) return 255;
                        if (rule >= 4) {
                            if (!voxel_util::wave(rule == 5 ? time * 0.001f : 1.0f, x, y, z, 8))
                                return rule == 4 ? 1 : 0;
                        }
                        return voxel_util::hash(glm::ivec3(x, y, z));
                    });
                    if (scene->rule == 3) {
                        checkpoint = {*elapsed, camera->mWorld};
                    } else {
                        camera->mWorld.SetTranslate(scene->cam.pos);
                        camera->mWorld.LookAt(scene->cam.look_at, Direction::up);
                    }
                }
                scene->orbit = scene->rule == 5;
                settings.free_view = scene->rule == 3;
                if (scene->rule == 3) {
                    camera->mWorld.SetTranslate(checkpoint.cam.GetTranslation() * 2.0f);
                    playerBody->transform.setPos(checkpoint.cam.GetTranslation() * 1.04f - glm::vec3(0, 0.1, 0));
                    playerBody->transform.rotateAroundY(0.0005f * (checkpoint.time - time));
                    playerBody->transform.lookAt(scene->cam.look_at, Direction::up);
                }
                break;
                // wave/zoom in/switch to FVTA/show different shaders
            case SCENES::FVTA:
                if (scene->ruled_changed) {
                    volume0->updateVoxels([](int x, int y, int z, GLubyte prev) {
                        if (!voxel_util::wave(1.0f, x, y, z, 8)) return 0;
                        return voxel_util::hash(glm::ivec3(x, y, z));
                    });
                }
                if (scene->rule == 3) {
                    scene->shader_setting = fvta_step_material; //show fvta
                } else {
                    scene->shader_setting = fixed_step_material;
                    camera->mWorld.SetTranslate(scene->cam.pos);
                    camera->mWorld.LookAt(scene->cam.look_at, Direction::up);

                }
                if (scene->rule > 1)
                    lookAtBlock(checkpoint, glm::vec3(8, 6, 8), glm::vec3(12, 8, 12));
                else {
                    camera->mWorld.SetTranslate(scene->cam.pos);
                    camera->mWorld.LookAt(scene->cam.look_at, Direction::up);
                    checkpoint = {*elapsed, camera->mWorld};
                }
                break;
            case SCENES::SHADERS:
                if (scene->ruled_changed) {
                    scene->shader_setting = (shader_setting_t) rule;
                    volume0->updateVoxels([](int x, int y, int z, GLubyte prev) {
                        if (!voxel_util::wave(1.0f, x, y, z, 8)) return 0;
                        return voxel_util::hash(glm::ivec3(x, y, z));
                    });
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
                break;
            case SCENES::LARGER:
                if (scene->ruled_changed) {
                    volume0->updateVoxels([](int x, int y, int z, GLubyte prev) {
                        if (!voxel_util::wave(1.0f, x, y, z, 64)) return 0;
                        return voxel_util::hash(glm::ivec3(x, y, z));
                    });
                }
                break;
            case SCENES::SDF:
                if (scene->ruled_changed) {
                    checkpoint = {*elapsed, camera->mWorld};
                    shader_setting_t shaders[] = {
                            fvta_step_pixel_pos,
                            fvta_step_shaded,
                    };
                    scene->shader_setting = shaders[(rule - 1) % 2];
                }
                if (rule == 1 || rule == 2) { //sphere
                    auto r =
                            size.x / 2 * glm::smoothstep(checkpoint.time, checkpoint.time + 1000, *elapsed);
                    volume0->updateVoxels([r, size](int x, int y, int z, GLubyte prev) {
                        if (!SDF::sphere(glm::vec3(x, y, z), size / 2.f, r)) return 0;
                        return voxel_util::hash(glm::ivec3(x, y, z));
                    });
                }
                if (rule == 3 || rule == 4) {
                    auto r =
                            size.x / 2.5 * glm::smoothstep(checkpoint.time, checkpoint.time + 1000, *elapsed);
                    volume0->updateVoxels([r, size](int x, int y, int z, GLubyte prev) {
                        if (!SDF::torus(glm::vec3(x, y, z), size / 2.f, glm::vec2(r, r / 3))) return 0;
                        return voxel_util::hash(glm::ivec3(x, y, z));
                    });
                }
                if (rule == 5 || rule == 6) {
                    auto r =
                            size.x / 3 * glm::smoothstep(checkpoint.time, checkpoint.time + 1000, *elapsed);
                    volume0->updateVoxels([r, size](int x, int y, int z, GLubyte prev) {
                        if (!SDF::octahedron(glm::vec3(x, y, z), size / 2.f, r)) return 0;
                        return voxel_util::hash(glm::ivec3(x, y, z));
                    });
                }
                break;
            case SCENES::CA:
                if (scene->ruled_changed) {
                    checkpoint = {*elapsed, camera->mWorld};
                    auto ca_setting = ca_settings[rule - 1];
                    auto ca_rule = cellularAutomata::CARules[ca_setting.rule];
                    delete this->ca3d;
                    this->ca3d = new cellularAutomata(ca_rule.state, size,
                                                      size * ca_setting.randomStateSizePercentage,
                                                      ca_setting.colorPalette,
                                                      ca_setting.drawMode);
                    volume0->generateColorPalette(ca3d->colorPalette, glm::ivec2(0, 255));
                }
                if (*elapsed > checkpoint.time + 40) {
                    checkpoint.time = *elapsed;
                    //volume0->cleanVoxel(); // not needed, we are updating the whole volume
                    ca3d->updateCells(cellularAutomata::CARules[rule - 1].survival,
                                      cellularAutomata::CARules[rule - 1].spawn);
                    volume0->updateVoxels([this](int x, int y, int z, GLubyte prev) {
                        return ca3d->findColorIndex(glm::vec3(x, y, z));
                    });
                }
                break;
            case SCENES::NOISE:
                if (scene->ruled_changed) {
                    checkpoint = {.time=*elapsed, .terrain_offset=glm::vec2(0, 0)};
                    volume0->generateColorPalette(colorPalette::terrainDefaultColors, glm::ivec2(0, 255));
                    //delete t;
                    t = new terrain(size.x * 4, size.z * 4, size.y * 0.7, 1.0f, 4, 0.5f, 2.0f, rule,
                                    glm::ivec2(32, 32));

                    volume0->updateVoxels([this](int x, int y, int z, GLubyte prev) {
                        // rule = 1 still, rule = 2 animated
                        // use time as offset to animate the terrain
                        return t->height2ColorIndex(x, y, z, glm::vec2(0, 255));
                    });
                }
                // offset the terrain with time
                if (rule > 3 && *elapsed > checkpoint.time + 40) {
                    volume0->setLOD(1);
                    volume0->generateColorPalette(colorPalette::terrainDefaultColors, glm::ivec2(0, 255));
                    checkpoint.time = *elapsed;
                    glm::vec2 offset = checkpoint.terrain_offset;
                    volume0->updateVoxels([offset, this](int x, int y, int z, GLubyte prev) {
                        // rule = 1 still, rule = 2 animated
                        // use time as offset to animate the terrain
                        return t->height2ColorIndex(x + offset.x, y, z + offset.y, glm::vec2(0, 255));
                    });
                    checkpoint.terrain_offset += glm::vec2(1, 1);
                }
                else {
                    int lods[] = {1,2,4};
                    volume0->setLOD(lods[rule-1]);
                }
                break;
            case SCENES::MINECRAFT:
                if (scene->ruled_changed) {
                    // create volumes
                    // figure out which volumes to add/remove
                    if (rule == renderer->numberVolumes()) break;
                    while (rule < renderer->numberVolumes())
                        // remove volume
                        renderer->removeVolume(renderer->numberVolumes() - 1);

                    scene->voxel_count = scene->volume_size * scene->volume_size * scene->volume_size * scene->rule;

                    auto center = Transform().translate(glm::vec3(-1.5)).scale(3.0f);
                    int size = scene->volume_size;
                    while (rule > renderer->numberVolumes()) {
                        //add volume
                        int n = renderer->numberVolumes();
                        if (!terrainGenerator->terrainExists(n)) {
                            rule--;
                            continue;
                        }
                        auto terrain = terrainGenerator->getTerrain(n);
                        auto pos = voxel_util::get_chunk_offset(n);
                        auto vol = new VoxelVolume(size, size, size,
                                                   center.translatedX(pos.x * 3 - 6).translateZ(pos.y * 3));
                        vol->updateVoxels([terrain, pos, size](int x, int y, int z, GLubyte prev) {
                            //return terrain->getHeight(x, z);
                            //printf("x: %d, y: %d, z: %d\n", x, y, z);
                            return terrain->height2ColorIndex(x + pos.x * size, y, z + pos.y * size, glm::vec2(0, 255));
                            //return voxel_util::hash(glm::ivec3(x, y, z));
                        });
                        renderer->add_volume(vol);
                    }
                }
                break;
        }

        if (scene->ruled_changed) scene->ruled_changed = false;
        if (getKey(GLFW_KEY_DOWN)) rule -= 1;
        if (getKey(GLFW_KEY_UP)) rule += 1;
        rule = glm::clamp(rule, 1, scene->highest_rule);
        if (rule != scene->rule) {
            scene->rule = rule;
            scene->ruled_changed = true;
        }

        int shader = static_cast<int>(scene->shader_setting);
        if (getKey(GLFW_KEY_COMMA)) shader--;
        if (getKey(GLFW_KEY_PERIOD)) shader++;
        shader = glm::clamp(shader, 0, (int) NBR_SHADER_SETTINGS - 1);
        scene->shader_setting = static_cast<shader_setting_t>(shader);
        settings.shader_setting = scene->shader_setting;

        if (getKey(GLFW_KEY_LEFT)) current_scene -= 1;
        if (getKey(GLFW_KEY_RIGHT)) current_scene += 1;
        current_scene = glm::clamp(current_scene, 0, (int) NBR_SCENES - 1);
        // will set scene when scene nbr change
        if (current_scene != scene->nbr)
            setScene(current_scene);

        if (scene->orbit)
            renderer->
                    orbit(deltaTimeUs
                                  .

                                          count()

                          * 0.001f);

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
                renderer->setShaderSetting(settings.shader_setting);
                gpu_time = renderer->render(
                        camera->GetWorldToClipMatrix(),
                        settings.free_view ? playerBody->transform.getPos() : camera->mWorld.GetTranslation(),
                        show_basis,
                        basis_length_scale,
                        basis_thickness_scale,
                        scene->lod);
                cpu_time = dt - gpu_time;
                if (settings.free_view)
                    playerBody->render(
                            camera->GetWorldToClipMatrix(),
                            glm::mat4(1.0), true, basis_length_scale, basis_thickness_scale
                    );
                ui->resize();
                if (settings.show_fps) ui->fps(gpu_time, cpu_time, 10);
                if (settings.show_crosshair) ui->crosshair();
                ui->displaySceneSettings(scene, *elapsed);
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

private:

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
