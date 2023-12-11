//
// Created by Theodor Lundqvist on 2023-12-06.
//
#pragma once
typedef struct settings_t{
    float edit_size = 0.8f;
    float edit_cooldown = 50.0f;
} settings_t;

typedef struct shader_setting_manager {
    bool is_using_FVTA = false;//to use fvta is true
    int choose_color = 0;// different color choice. 
                         // 0->fixed step + pixel_pos
                         // 1->fvta  step + pixel_pos
                         // 2->fvta  step + voxel_pos
                         // 3->fvta  step + normal
                         // 4->fvta  step + uvw
                         // 5->fvta  step + uv
                         // 6->fvta  step + depth
                         // 7->fvta  step + shaded
                         // 8->fvta  step + shaded with simple AO
                         // KEY LEFT -> number --, KEY RIGHT -> number ++
} shader_setting_manager;
