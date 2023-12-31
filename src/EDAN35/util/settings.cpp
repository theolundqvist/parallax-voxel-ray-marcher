//
// Created by Theodor Lundqvist on 2023-12-06.
//
#pragma once

enum shader_setting_t
{
    fixed_step_material = 0,
    fvta_step_material,
    fvta_step_pixel_pos,
    fvta_step_voxel_pos,
    fvta_step_normal,
    fvta_tep_uvw,
    fvta_step_uv,
    fvta_step_depth,
    fvta_step_shaded,
    fvta_step_shaded_with_simple_AO,
    fvta_step_colorpalette,
    fvta_step_shaded_colorpalette,
    fvta_step_shaded_colorpalette_AO,
    NBR_SHADER_SETTINGS
};

typedef struct settings_t{
    float edit_size = 0.8f;
    float edit_cooldown = 50.0f;
    bool show_crosshair = false;
    bool drag_to_move = true;
    bool show_fps = true;
    bool free_view = false;
    shader_setting_t shader_setting = fixed_step_material;
} settings_t;
