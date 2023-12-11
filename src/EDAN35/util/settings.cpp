//
// Created by Theodor Lundqvist on 2023-12-06.
//
#pragma once
typedef struct settings_t{
    float edit_size = 0.8f;
    float edit_cooldown = 50.0f;
    bool show_crosshair = false;
    bool drag_to_move = true;
    bool show_fps = true;
    bool free_view = false;
} settings_t;
enum shader_setting
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
    fvta_step_shaded_with_simple_AO
};