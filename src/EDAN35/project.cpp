
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
#include "project/DemoApp.cpp"
#include "world/WorldApp.hpp"
#include <charconv>
#include <cstdlib>
#include <string_view>

#include <list>
#include <map>
#include <memory>

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
                                             config::msaa_rate, false, true, WindowManager::SwapStrategy::disable_vsync);

    if (window == nullptr) {
        throw std::runtime_error("Failed to get a window: aborting!");
    }

    bonobo::init();
}

edan35::Project::~Project() { bonobo::deinit(); }

void edan35::Project::run(bool demo, std::filesystem::path const& worldPath,
                         std::optional<std::uint64_t> seed) {
    // Set up the camera
    mCamera.mWorld.SetTranslate(glm::vec3(0.0f, 0.0f, 6.0f));
    mCamera.mMouseSensitivity = glm::vec2(0.003f);
    mCamera.mMovementSpeed = glm::vec3(3.0f); // 3 m/s => 10.8 km/h
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
    float elapsed_time_ms = 0.0f;
    // GameObject::addShaderToLibrary(&program_manager, "phong",
    // phong_set_uniforms); GameObject::addShaderToLibrary(&program_manager,
    // "enemy", phong_set_uniforms);
    // GameObject::addShaderToLibrary(&program_manager, "powerup",
    //                                phong_set_uniforms);
    // GameObject::addShaderToLibrary(&program_manager, "shield",
    //                                phong_set_uniforms);

    std::unique_ptr<DemoApp> demoApp;
    std::unique_ptr<world::WorldApp> worldApp;
    if (demo) demoApp = std::make_unique<DemoApp>(window, &mCamera, &inputHandler, &program_manager, &elapsed_time_ms);
    else worldApp = std::make_unique<world::WorldApp>(window, &mCamera, &inputHandler, &program_manager, worldPath, seed);


    glClearDepthf(1.0f);
    glClearColor(0.85f, 0.85f, 0.74f, 1.0f);
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
        float dt = deltaTimeUs.count() * 0.001f;
        lastTime = nowTime;
        elapsed_time_ms += dt;

        auto &io = ImGui::GetIO();
        inputHandler.SetUICapture(io.WantCaptureMouse, io.WantCaptureKeyboard);

        glfwPollEvents();
        inputHandler.Advance();
        if (worldApp) worldApp->update(deltaTimeUs);
        else demoApp->update(deltaTimeUs);

        if (inputHandler.GetKeycodeState(GLFW_KEY_R) & JUST_PRESSED) {
            shader_reload_failed = !program_manager.ReloadAllPrograms();
            if (worldApp && !shader_reload_failed) worldApp->refreshPrograms();
            if (shader_reload_failed) {
                LogError("Shader reload failed; fix the shader and press R to retry.");
                show_logs = true;
            }
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
                polygon_mode = bonobo::polygon_mode_t::fill;;
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            }
        }

        mWindowManager.NewImGuiFrame();

        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // RENDER
        if (worldApp) worldApp->render(show_basis, basis_length_scale, basis_thickness_scale, dt);
        else demoApp->render(show_basis, basis_length_scale, basis_thickness_scale, dt);

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
        //const auto now = std::chrono::high_resolution_clock::now();
        glfwSwapBuffers(window);
        //const auto end = std::chrono::high_resolution_clock::now();
        //printf("frame time: %f ms\n", std::chrono::duration<float>(end - now).count() * 1000.0f);
    }
}

int main(int argc, char** argv) {
    std::setlocale(LC_ALL, "");
    try {
        bool demo = false;
        std::filesystem::path worldPath;
        std::optional<std::uint64_t> seed;
        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "--help") {
                std::cout << "EDAN35_Project [--world PATH] [--seed INTEGER] [--demo]\n"
                             "Floating islands: WASD fly, Shift sprint, Space carve, X build, Esc menu.\n"
                             "Edits are saved before becoming visible. --demo opens the original scenes.\n";
                return 0;
            }
            if (arg == "--demo") { demo = true; continue; }
            if ((arg != "--world" && arg != "--seed") || i + 1 == argc)
                throw std::runtime_error("Expected --world PATH, --seed INTEGER, or --demo");
            std::string_view value = argv[++i];
            if (arg == "--world") worldPath = value;
            else {
                std::uint64_t parsed = 0;
                auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
                if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
                    throw std::runtime_error("Seed must be an unsigned 64-bit integer");
                seed = parsed;
            }
        }
        if (!demo && worldPath.empty()) {
#if defined(_WIN32)
            auto root = std::getenv("LOCALAPPDATA");
            if (!root) throw std::runtime_error("LOCALAPPDATA unavailable; pass --world PATH");
            worldPath = std::filesystem::path(root) / "ParallaxVoxel" / "worlds" / "islands";
#else
            auto home = std::getenv("HOME");
            if (!home) throw std::runtime_error("HOME unavailable; pass --world PATH");
#if defined(__APPLE__)
            worldPath = std::filesystem::path(home) / "Library" / "Application Support" / "ParallaxVoxel" / "worlds" / "islands";
#else
            auto data = std::getenv("XDG_DATA_HOME");
            worldPath = (data && *data ? std::filesystem::path(data) : std::filesystem::path(home) / ".local" / "share")
                        / "parallax-voxel" / "worlds" / "islands";
#endif
#endif
        }
        Bonobo framework;
        edan35::Project project(framework.GetWindowManager());
        project.run(demo, worldPath, seed);
    } catch (std::exception const& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
