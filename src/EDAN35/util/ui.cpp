#pragma once

#include "config.hpp"
#include <glm/glm.hpp>
#include <imgui.h>
#include "AppState.cpp"
#include "settings.cpp"

class UI {
public:
    ImFont *font1;
    ImGuiIO io;
    GLFWwindow *window = nullptr;
    float font_height = 0.0f;
    glm::ivec2 window_scale;
    glm::ivec2 window_size;

    UI(GLFWwindow *window) {
        io = ImGui::GetIO();
        font_height = config::resolution_x/10.0f;
        font1 = io.Fonts->AddFontFromFileTTF(
                config::resources_path("fonts/Poppins-Regular.ttf").c_str(), font_height);
        this->window = window;
    }


    void resize() {
        int w, h;
        float xscale, yscale;
        glfwGetFramebufferSize(window, &w, &h);
        glfwGetWindowContentScale(window, &xscale, &yscale);
        window_scale = glm::ivec2(xscale, yscale);
        window_size = glm::ivec2(w, h);
        //printf("fbSize=%dx%d, scale=%.2fx%.2f\n", w, h, xscale, yscale);
        ImGui::SetWindowSize(ImVec2((float)w/xscale, (float)h/yscale));
    }


    std::list<float> gpu_times = {};
    std::list<float> cpu_times = {};
    void fps(float gpu_time, float cpu_time, int sliding_mean = 10) {
        gpu_times.push_back(gpu_time);
        cpu_times.push_back(cpu_time);
        if(gpu_times.size() > sliding_mean) {
            gpu_times.pop_front();
            cpu_times.pop_front();
        }
        gpu_time = 0.0;
        cpu_time = 0.0;
        for(auto time : gpu_times) {
            gpu_time += time;
        }
        for(auto time : cpu_times) {
            cpu_time += time;
        }
        gpu_time /= (float)gpu_times.size();
        cpu_time /= (float)cpu_times.size();
        auto flags = ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground;
        ImGui::Begin("fps", nullptr, flags);
        ImGui::SetWindowPos(ImVec2(0,0));
        ImGui::SetWindowFontScale(0.3f);
        ImGui::PushFont(font1);
        ImGui::TextColored(ImColor(0,0,0),"FPS: %.0f", ImGui::GetIO().Framerate);
        ImGui::TextColored(ImColor(0,0,0),"GPU: %.2f ms", gpu_time);
        ImGui::TextColored(ImColor(0,0,0), "CPU: %.2f ms", cpu_time);
        ImGui::PopFont();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::End();
    }


    void displaySettings(settings_t settings){
        auto flags = ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground;
        ImGui::Begin("settings", nullptr, flags);
        int h = window_size.y / window_scale.y;
        int number_rows = 3;
        float font_scale = 0.25f;
        ImGui::SetWindowPos(ImVec2(0,h - font_height * font_scale * (number_rows + 1)));
        ImGui::SetWindowFontScale(font_scale);
        ImGui::PushFont(font1);
        ImGui::TextColored(ImColor(0,0,0),"Settings");
        ImGui::TextColored(ImColor(0,0,0),"Edit size: %.2f m", settings.edit_size);
        ImGui::TextColored(ImColor(0,0,0),"Edit cooldown: %.0f ms", settings.edit_cooldown);
        //ImGui::TextColored(ImColor(0,0,0),"Voxel count: %d", settings.voxel_count);
        //ImGui::TextColored(ImColor(0,0,0),"Voxel count: %d", settings.voxel_count);
        //ImGui::TextColored(ImColor(0,0,0),"Voxel count: %d", settings.voxel_count);
        //ImGui::TextColored(ImColor(0,0,0),"Voxel count: %d", settings.voxel_count);
        //ImGui::TextColored(ImColor(0,0,0),"Voxel count: %d", settings.voxel_count);
        //ImGui::TextColored(ImColor(0,0,0),"Voxel count: %d", settings.voxel_count);
        //ImGui::TextColored(ImColor(0,0,0),"Voxel count: %d", settings.voxel_count);
        ImGui::PopFont();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::End();
    }
    void crosshair() {
        float crosshairSize = 10.0f;
        float lineWidth = 1.5f;
        float gapSize = 4.0f;
        float dotSize = 2.0f;

        ImDrawList *drawList = ImGui::GetBackgroundDrawList();
        auto white = ImColor(255, 255, 255);
        DrawCrossHair(drawList, crosshairSize + 1.0f, lineWidth + 0.5f, gapSize - 0.5f, dotSize + 0.5f, white);

        auto black = ImColor(0, 0, 0);
        DrawCrossHair(drawList, crosshairSize, lineWidth, gapSize, dotSize, black);

        //Rendering :)
        ImGui::Render();
    }

    void pauseMenu() {
        auto center = glm::vec2(ImGui::GetWindowWidth(), ImGui::GetWindowHeight()) * 0.5f;
        TextBox("paused", center + glm::vec2(0, -200));
        if (Button("Continue (esc)", center)) {
            state = RUNNING;
        }
        if (Button("Quit (q)", center + glm::vec2(0, +200))) {
            exit(0);
        }
    }

private:
    void CenterCursor(std::string text) {
        auto windowWidth = ImGui::GetWindowWidth();
        auto textWidth = ImGui::CalcTextSize(text.c_str()).x;

        ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
    }

    void Begin(std::string name, bool background, glm::vec2 pos) {
        auto flags = ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoTitleBar;
        if (!background)
            flags |= ImGuiWindowFlags_NoBackground;
        ImGui::Begin(name.c_str(), nullptr, flags);
        SetWindowPos(pos);
    }

    void DrawCrossHair(ImDrawList *drawList, float crosshairSize, float lineWidth, float gapSize, float dotSize,
                       ImColor color) {
        auto center = ImGui::GetWindowSize();
        float centerX = center.x / 2.0f;
        float centerY = center.y / 2.0f;
        // Draw horizontal line
        drawList->AddLine({centerX - crosshairSize - gapSize, centerY}, {centerX - gapSize, centerY}, color, lineWidth);
        drawList->AddLine({centerX + gapSize, centerY}, {centerX + crosshairSize + gapSize, centerY}, color, lineWidth);

        // Draw vertical line
        drawList->AddLine({centerX, centerY - crosshairSize - gapSize}, {centerX, centerY - gapSize}, color, lineWidth);
        drawList->AddLine({centerX, centerY + gapSize}, {centerX, centerY + crosshairSize + gapSize}, color, lineWidth);

        // Draw dot in the center
        //drawList->AddCircleFilled({ centerX, centerY }, dotSize, color);
    }

    void End() { ImGui::End(); }

    void SetWindowPos(glm::vec2 pos) {
        auto size = ImGui::GetWindowSize();
        ImGui::SetWindowPos(ImVec2(pos.x - size.x * .5f, pos.y - size.y * .5f));
    }

    void TextBox(std::string text, glm::vec2 pos) {
        text = " " + text + " ";
        Begin(text, true, pos);

        // center
        ImGui::PushFont(font1);
        ImGui::SetWindowFontScale(0.7f);
        CenterCursor(text);
        ImGui::Text("%s", text.c_str());
        ImGui::PopFont();
        ImGui::SetWindowFontScale(1.0f);
        End();
    }

    bool Button(std::string text, glm::vec2 pos) {
        text = "   " + text + "   ";
        Begin(text, false, pos);
        ImGui::PushFont(font1);
        CenterCursor(text);
        auto clicked = ImGui::Button(text.c_str());
        ImGui::PopFont();
        End();
        return clicked;
    }

};
