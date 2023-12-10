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
    QUAD,
    CUBE,
    NINE_BY_NINE,
};

cam_pos_t camera_positions[3] = {
        {glm::vec3(-0.15, -0.65, 6.5),   glm::vec3(0, 0, 0)},
        {glm::vec3(4., 3.35, -5.5), glm::vec3(0)},
        {glm::vec3(0.15, 6., 17.5), glm::vec3(3, 0, 3)},
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
    bool free_view = false;
    int rule = 1;
    int voxel_count = 0;
    bool ruled_changed = false;
} scene_settings_t;

scene_settings_t scenes[] = {
        {0, "Quad fixed",     camera_positions[QUAD],         3, 0},
        {1, "Cube fixed",     camera_positions[CUBE],         4, 0},
        {2, "Cube FVTA step", camera_positions[CUBE],         4, 1},
        {3, "Shaders",        camera_positions[CUBE],         3, 1},
        {4, "Free view",      camera_positions[CUBE],         1, 16,  1, 0,  false, true},
        {5, "Larger",         camera_positions[CUBE],         1, 128, 1, 0,  true},
        {6, "SDF",            camera_positions[CUBE],         4, 128, 1, 3,  true},
        {7, "CA",             camera_positions[CUBE],         4, 128, 1, 3,  true},
        {8, "Noise",          camera_positions[CUBE],         3, 128, 1, 0,  true},
        {9, "Minecraft",      camera_positions[NINE_BY_NINE], 10, 128, 1, 20, false, false, 3},
};

enum SCENES {
    QUAD_FIXED,
    CUBE_FIXED,
    CUBE_FVTA_STEP,
    SHADERS,
    FREE_VIEW,
    LARGER,
    SDF,
    CA,
    NOISE,
    MINECRAFT,
    NBR_SCENES
};
