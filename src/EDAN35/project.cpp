
#include "project.hpp"

#include "config.hpp"
#include "core/Bonobo.h"
#include "core/FPSCamera.h"
#include "core/ShaderProgramManager.hpp"
#include "core/helpers.hpp"
#include "core/node.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <tinyfiledialogs.h>

#include <clocale>
#include <stdexcept>

#include "project/App.cpp"

#include <list>
#include <map>

edan35::Project::Project(WindowManager &windowManager)
    : mCamera(0.5f * glm::half_pi<float>(),
              static_cast<float>(config::resolution_x) /
                  static_cast<float>(config::resolution_y),
              0.01f, 1000.0f),
      inputHandler(), mWindowManager(windowManager), window(nullptr) {
  WindowManager::WindowDatum window_datum{inputHandler,
                                          mCamera,
                                          config::resolution_x,
                                          config::resolution_y,
                                          0,
                                          0,
                                          0,
                                          0};

  window = mWindowManager.CreateGLFWWindow("EDAN35: Assignment 5", window_datum,
                                           config::msaa_rate);

  if (window == nullptr) {
    throw std::runtime_error("Failed to get a window: aborting!");
  }

  bonobo::init();
}

edan35::Project::~Project() { bonobo::deinit(); }

void edan35::Project::run() {
  // Set up the camera
  mCamera.mWorld.SetTranslate(glm::vec3(0.0f, 0.0f, 6.0f));
  mCamera.mMouseSensitivity = glm::vec2(0.003f);
  mCamera.mMovementSpeed = glm::vec3(6.0f); // 3 m/s => 10.8 km/h
  //
  // Load all textures.
  //
  // GLuint const sun_texture =
  //     bonobo::loadTexture2D(config::resources_path("planets/2k_sun.jpg"));
  // GLuint const mercury_texture =
  //     bonobo::loadTexture2D(config::resources_path("planets/2k_mercury.jpg"));
  // GLuint const venus_texture = bonobo::loadTexture2D(
  //     config::resources_path("planets/2k_venus_atmosphere.jpg"));
  // GLuint const mars_texture =
  //     bonobo::loadTexture2D(config::resources_path("planets/2k_mars.jpg"));
  // GLuint const jupiter_texture =
  //     bonobo::loadTexture2D(config::resources_path("planets/2k_jupiter.jpg"));
  // GLuint const saturn_texture =
  //     bonobo::loadTexture2D(config::resources_path("planets/2k_saturn.jpg"));
  // GLuint const uranus_texture =
  //     bonobo::loadTexture2D(config::resources_path("planets/2k_uranus.jpg"));
  // GLuint const neptune_texture =
  //     bonobo::loadTexture2D(config::resources_path("planets/2k_neptune.jpg"));
  // GLuint const earth_texture = bonobo::loadTexture2D(
  //     config::resources_path("planets/2k_earth_daymap.jpg"));
  // GLuint const moon_texture =
  //     bonobo::loadTexture2D(config::resources_path("planets/2k_moon.jpg"));
  //
  // bonobo::material_data material;
  // material.ambient = glm::vec3(0.4f, 0.4f, 0.4f);
  // material.diffuse = glm::vec3(0.7f, 0.2f, 0.4f);
  // material.specular = glm::vec3(1.0f, 1.0f, 1.0f);
  // material.shininess = 10.0f;
  //
  // std::vector<texture_program *> textures = {
  //     new texture_program{"Sun", &sun_texture, material},
  //     new texture_program{"Mercury", &mercury_texture, material},
  //     new texture_program{"Venus", &venus_texture, material},
  //     new texture_program{"Mars", &mars_texture, material},
  //     new texture_program{"Jupiter", &jupiter_texture, material},
  //     new texture_program{"Saturn", &saturn_texture, material},
  //     new texture_program{"Uranus", &uranus_texture, material},
  //     new texture_program{"Neptune", &neptune_texture, material},
  //     new texture_program{"Earth", &earth_texture, material},
  //     new texture_program{"Moon", &moon_texture, material}};
  //
  // GameObject::setTextureLibrary(textures);
  //
  // Create the shader programs
  ShaderProgramManager program_manager;
  auto light_position = glm::vec3(-2.0f, 4.0f, 2.0f);
  bool use_normal_mapping = false;
  auto camera_position = mCamera.mWorld.GetTranslation();
  float elapsed_time_s = 0.0f;
  // GameObject::addShaderToLibrary(&program_manager, "phong",
  // phong_set_uniforms); GameObject::addShaderToLibrary(&program_manager,
  // "enemy", phong_set_uniforms);
  // GameObject::addShaderToLibrary(&program_manager, "powerup",
  //                                phong_set_uniforms);
  // GameObject::addShaderToLibrary(&program_manager, "shield",
  //                                phong_set_uniforms);

  auto app = new App(window, &mCamera, &inputHandler, &program_manager, &elapsed_time_s);


  glClearDepthf(1.0f);
  glClearColor(1.0f, 1.0f, 0.94f, 1.0f);
  glEnable(GL_DEPTH_TEST);

  auto lastTime = std::chrono::high_resolution_clock::now();

  bool show_logs = false;
  bool show_gui = true;
  bool shader_reload_failed = false;
  bool show_basis = false;
  auto polygon_mode = bonobo::polygon_mode_t::fill;
  float basis_thickness_scale = 0.2f;
  float basis_length_scale = 1.0f;
  bool camera_free_view = false;
  bool hideMouse = false;
  while (!glfwWindowShouldClose(window)) {
    auto const nowTime = std::chrono::high_resolution_clock::now();
    auto const deltaTimeUs =
        std::chrono::duration_cast<std::chrono::microseconds>(nowTime -
                                                              lastTime);
    float dt = deltaTimeUs.count() / 1000.0;
    lastTime = nowTime;
    elapsed_time_s += dt;

    auto &io = ImGui::GetIO();
    inputHandler.SetUICapture(io.WantCaptureMouse, io.WantCaptureKeyboard);

    glfwPollEvents();
    inputHandler.Advance();
    app->update(deltaTimeUs);

    if (inputHandler.GetKeycodeState(GLFW_KEY_R) & JUST_PRESSED) {
      shader_reload_failed = !program_manager.ReloadAllPrograms();
      if (shader_reload_failed)
        tinyfd_notifyPopup("Shader Program Reload Error",
                           "An error occurred while reloading shader programs; "
                           "see the logs for details.\n"
                           "Rendering is suspended until the issue is solved. "
                           "Once fixed, just reload the shaders again.",
                           "error");
    }
    if (inputHandler.GetKeycodeState(GLFW_KEY_F3) & JUST_RELEASED)
      show_logs = !show_logs;
    if (inputHandler.GetKeycodeState(GLFW_KEY_F2) & JUST_RELEASED)
      show_gui = !show_gui;
    if (inputHandler.GetKeycodeState(GLFW_KEY_F11) & JUST_RELEASED)
      mWindowManager.ToggleFullscreenStatusForWindow(window);

    // Retrieve the actual framebuffer size: for HiDPI monitors,
    // you might end up with a framebuffer larger than what you
    // actually asked for. For example, if you ask for a 1920x1080
    // framebuffer, you might get a 3840x2160 one instead.
    // Also it might change as the user drags the window between
    // monitors with different DPIs, or if the fullscreen status is
    // being toggled.
    int framebuffer_width, framebuffer_height;
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    glViewport(0, 0, framebuffer_width, framebuffer_height);

    //
    // Todo: If you need to handle inputs, you can do it here
    //
    if (inputHandler.GetKeycodeState(GLFW_KEY_B) & JUST_PRESSED)
      show_basis = !show_basis;
    if (inputHandler.GetKeycodeState(GLFW_KEY_M) & JUST_PRESSED) {
      if (polygon_mode == bonobo::polygon_mode_t::fill) {
        polygon_mode = bonobo::polygon_mode_t::line;
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      } else {
        polygon_mode = bonobo::polygon_mode_t::fill;
        ;
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      }
    }

    mWindowManager.NewImGuiFrame();

    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    // RENDER
    app->render(show_basis, basis_length_scale, basis_thickness_scale);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    //
    // Todo: If you want a custom ImGUI window, you can set it up
    //       here
    //
    //
    bonobo::changePolygonMode(polygon_mode);

    if (show_basis)
      bonobo::renderBasis(basis_thickness_scale, basis_length_scale,
                          mCamera.GetWorldToClipMatrix());
    if (show_logs) {
      ImGui::SetWindowFontScale(0.1f);
      Log::View::Render();
      ImGui::SetWindowFontScale(1.0f);
    }
    mWindowManager.RenderImGuiFrame(show_gui);

    glfwSwapBuffers(window);
  }
}

int main() {
  std::setlocale(LC_ALL, "");

  Bonobo framework;

  try {
    edan35::Project project(framework.GetWindowManager());
    project.run();
  } catch (std::runtime_error const &e) {
    LogError(e.what());
  }
}
