//
// Created by Theodor Lundqvist on 2023-12-06.
//
#pragma once

#include "../util/Direction.cpp"
#include "../util/GameObject.cpp"
#include "../util/parametric_shapes.cpp"
#include "../util/parametric_shapes.hpp"
#include "../util/ui.cpp"
#include "core/FPSCamera.h"
#include <cstddef>
#include <glm/gtc/type_ptr.hpp>
#include <list>
#include "../util/AppState.cpp"
#include <map>
#include <iostream>

typedef struct cam_pos_t {
    glm::vec3 pos;
    glm::vec3 look_at;
} cam_pos_t;

enum CAM {
    QUAD_CAM,
    CUBE_CAM,
    MC_CAM,
};

cam_pos_t camera_positions[3] = {
        {glm::vec3(-0.15, -0.65, 6.5), glm::vec3(0, 0, 0)},
        {glm::vec3(4., 3.35, -5.5),    glm::vec3(0)},
        {glm::vec3(0, 4, -7.5),    glm::vec3(0, 1, 0)},
};

typedef struct scene_settings_t {
    int nbr;
    std::string name;
    cam_pos_t cam;
    int highest_rule = 1;
    int shader_setting = 1;
    int volume_size = 16;
    int volumes = 1;
    bool orbit = false;
    int rule = 1;
    int voxel_count = 0;
    bool ruled_changed = true;
} scene_settings_t;

scene_settings_t scenes[] = {
        {0, "Quad fixed",     camera_positions[QUAD_CAM],         4,  0},
        {1, "Cube fixed",     camera_positions[CUBE_CAM],         5,  0},
        {2, "Cube FVTA step", camera_positions[CUBE_CAM],         3,  0},
        {3, "Shaders",        camera_positions[CUBE_CAM],         3,  1},
        {4, "Larger",         camera_positions[CUBE_CAM],         1,  1, 128, 1,  true},
        {5, "SDF",            camera_positions[CUBE_CAM],         4,  1, 128, 1,  true},
        {6, "CA",             camera_positions[CUBE_CAM],         4,  1, 50, 1,  true},
        {7, "Noise",          camera_positions[CUBE_CAM],         2,  1, 128, 1,  true},
        {8, "Minecraft",      camera_positions[MC_CAM], 100, 1, 128, 1,  false, 3},
};

enum SCENES {
    QUAD,
    CUBE,
    FVTA,
    SHADERS,
    LARGER,
    SDF,
    CA,
    NOISE,
    MINECRAFT,
    NBR_SCENES
};
