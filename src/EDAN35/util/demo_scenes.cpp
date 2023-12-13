//
// Created by Theodor Lundqvist on 2023-12-06.
//
#pragma once

#include "../util/Direction.cpp"
#include "../util/settings.cpp"
#include "../util/cellularAutomata.hpp"
#include "../util/colorPalette.hpp"
#include "../util/sdf.cpp"
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
    SHADERS_CAM,
    CA_CAM,
    MC_CAM,
};

cam_pos_t camera_positions[] = {
        {glm::vec3(-0.15, -0.65, 6.5), glm::vec3(0, 0, 0)},
        {glm::vec3(4., 3.35, -5.5),    glm::vec3(0)},
        {glm::vec3(2.0, 1.3, -3),      glm::vec3(0, -0.5, 0)},
        {glm::vec3(2.5, 2.4, -3.3),    glm::vec3(0)},
        {glm::vec3(-1.5, 4.8, -7.5),   glm::vec3(-1.5, 2, 0)},
};

typedef struct scene_settings_t {
    int nbr;
    std::string name;
    cam_pos_t cam;
    int highest_rule = 1;
    shader_setting_t shader_setting = shader_setting_t::fixed_step_material;
    int volume_size = 16;
    int volumes = 1;
    bool orbit = false;
    int rule = 1;
    int voxel_count = 0;
    bool ruled_changed = true;
    bool lod = false;
} scene_settings_t;


scene_settings_t scenes[] = {
        {0, "Quad fixed",     camera_positions[QUAD_CAM],    4,                              fixed_step_material},
        {1, "Cube fixed",     camera_positions[CUBE_CAM],    5,                              fixed_step_material},
        {2, "Cube FVTA step", camera_positions[CUBE_CAM],    3,                              fixed_step_material},
        {3, "Shaders",        camera_positions[SHADERS_CAM], NBR_SHADER_SETTINGS - 1,        fvta_step_material},
        {4, "Larger",         camera_positions[CUBE_CAM],    1,                              fvta_step_shaded_with_simple_AO, 128, 1, true},
        {5, "SDF",            camera_positions[CUBE_CAM],    6,                              fvta_step_pixel_pos, 128,  1, true},
        {6, "CA",             camera_positions[CA_CAM],    cellularAutomata::NBR_CA_RULES, fvta_step_shaded_colorpalette,   100,  1, true},
        {7, "Noise",          camera_positions[CUBE_CAM],    5,                              fvta_step_shaded_colorpalette,   100, 1, true},
        {8, "Minecraft",      camera_positions[MC_CAM],      100,                            fvta_step_shaded_colorpalette,   128, 1, false, 4, },
};

typedef struct ca_setting_t {
    cellularAutomata::CA_rule rule;
    float randomStateSizePercentage;
    shader_setting_t shader_setting;
    std::vector<glm::vec3> colorPalette;
    int drawMode;
} ca_setting_t;

// rule1: cloud, CAColorsBlue2Pink dis2colorIndex
// rule2: pyroclastic, CAColorsBlue2Pink hp2colorIndex
// rule3: 678, CAColorsRed2Green, density2colorIndex
// rule4: 445, CAColorsRed2Green, pos2colorIndex
ca_setting_t ca_settings[] = {
        {cellularAutomata::R_BUILDER,      0.35f, fvta_step_colorpalette, colorPalette::CAColorsBlue2Orange, cellularAutomata::drawModes::hp2color},
        {cellularAutomata::R_SPIKE_GROWTH, 0.15f, fvta_step_colorpalette, colorPalette::CAColorsBlue2Orange, cellularAutomata::drawModes::hp2color}, {cellularAutomata::R_445,          0.25f, fvta_step_colorpalette, colorPalette::CAColorsBlue2Orange, cellularAutomata::drawModes::hp2color},
        {cellularAutomata::R_678,          0.25f, fvta_step_colorpalette, colorPalette::CAColorsBlue2Orange, cellularAutomata::drawModes::density2color},
        // maybe remove pyroclastic
        {cellularAutomata::R_PYROCLASTIC,  0.4f,  fvta_step_colorpalette, colorPalette::CAColorsBlue2Orange, cellularAutomata::drawModes::density2color},
        // remove
        {cellularAutomata::R_CLOUD1,       0.15f, fvta_step_colorpalette, colorPalette::CAColorsBlue2Orange, cellularAutomata::drawModes::hp2color},
        // maybe remove
        {cellularAutomata::R_AMOEBA,       0.15f, fvta_step_colorpalette, colorPalette::CAColorsBlue2Orange, cellularAutomata::drawModes::hp2color},
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
