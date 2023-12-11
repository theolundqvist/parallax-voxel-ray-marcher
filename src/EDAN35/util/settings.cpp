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