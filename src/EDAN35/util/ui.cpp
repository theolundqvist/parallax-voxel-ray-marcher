#pragma once
#include "config.hpp"
#include <glm/glm.hpp>
#include <imgui.h>

class UI {
public:
  ImFont *font1;
  ImGuiIO io;
	int window_size;

  UI() {
    io = ImGui::GetIO();
    font1 = io.Fonts->AddFontFromFileTTF(
        config::resources_path("fonts/Poppins-Regular.ttf").c_str(), config::resolution_x/3);
  }

  void pauseMenu(std::function<void(void)> start){
    auto center = glm::vec2(config::resolution_x, config::resolution_y) * 0.5f;
    TextBox("paused", center + glm::vec2(0, -200));
    if (Button("Continue (esc)", center)) {
			start();
    }
    if (Button("Quit (q)", center + glm::vec2(0, +200))) {
      exit(0);
    }
  }

private:
  void CenterCursor(std::string text) {
    auto windowWidth = ImGui::GetWindowSize().x;
    auto textWidth = ImGui::CalcTextSize(text.c_str()).x;

    ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
  }
  void Begin(std::string name, bool background,glm::vec2 pos) {
    auto flags = ImGuiWindowFlags_NoDecoration |
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoTitleBar;
    if (!background)
      flags |= ImGuiWindowFlags_NoBackground;
    ImGui::Begin(name.c_str(), nullptr, flags);
    SetWindowPos(pos);
  }
  void End() { ImGui::End(); }

  void SetWindowPos(glm::vec2 pos) {
    auto size = ImGui::GetWindowSize();
    ImGui::SetWindowPos(ImVec2(pos.x - size.x * .5f, pos.y - size.y * .5f));
  }
  void TextBox(std::string text, glm::vec2 pos, float scale){
    text = " " + text + " ";
    Begin(text,true, pos);

    // center
    ImGui::PushFont(font1);
    ImGui::SetWindowFontScale(0.7f);
    CenterCursor(text);
    ImGui::Text("%s", text.c_str());
    ImGui::PopFont();
    ImGui::SetWindowFontScale(1.0f);
    End();
	}
  void TextBox(std::string text, glm::vec2 pos) { TextBox(text, pos, 1.0f); }
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
