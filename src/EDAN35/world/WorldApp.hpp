#pragma once

#include "Stream.hpp"
#include "Generate.hpp"
#include "../project/VoxelRenderer.cpp"
#include <imgui.h>
#include <map>
#include <set>
#include <span>

namespace world {
class WorldApp {
public:
    WorldApp(GLFWwindow* window, FPSCameraf* camera, InputHandler* input,
             ShaderProgramManager* programs, std::filesystem::path path,
             std::optional<std::uint64_t> seed)
        : window(window), camera(camera), input(input), path(std::move(path)), requestedSeed(seed),
          stream(std::make_unique<Stream>(this->path, seed)), palette(worldPalette()) {
        programs->CreateAndRegisterProgram("world-voxel",
            {{ShaderType::vertex, "EDAN35/voxel.vert"}, {ShaderType::fragment, "EDAN35/voxel.frag"}}, program);
        programs->CreateAndRegisterProgram("world-sky",
            {{ShaderType::vertex, "EDAN35/worldsky.vert"}, {ShaderType::fragment, "EDAN35/worldsky.frag"}}, skyProgram);
        if (program == 0 || skyProgram == 0) throw std::runtime_error("World shader compilation failed; see shader log");
        glGenVertexArrays(1, &skyVao);
        camera->SetFov(glm::radians(65.0f));
        camera->mMovementSpeed = glm::vec3(3.0f);
        camera->mWorld.SetTranslate(spawnPosition(seed.value_or(DefaultSeed)));
        camera->mWorld.LookAt(spawnTarget(seed.value_or(DefaultSeed)), glm::vec3(0, 1, 0));
    }
    ~WorldApp() {
        stream.reset();
        resident.clear();
        glDeleteVertexArrays(1, &skyVao);
    }

    void refreshPrograms() {
        for (auto& [key, volume] : resident) if (volume) volume->setProgram(program);
    }

    void update(std::chrono::microseconds delta) {
        auto state = stream->status();
        if (glfwWindowShouldClose(window)) {
            closing = true;
            glfwSetWindowShouldClose(window, GLFW_FALSE);
        }
        if (pressed(GLFW_KEY_ESCAPE)) paused = !paused;
        bool capture = !paused && !closing && state.ready && state.error.empty();
        if (capture != cursorCaptured) {
            cursorCaptured = capture;
            glfwSetInputMode(window, GLFW_CURSOR, capture ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            double x, y;
            glfwGetCursorPos(window, &x, &y);
            input->FeedMouseMotion(glm::vec2(x, y));
            camera->Update(std::chrono::microseconds(0), *input, true, true, false);
        }
        if (state.ready && !initialized) {
            initialized = true;
            camera->mWorld.SetTranslate(spawnPosition(state.seed));
            camera->mWorld.LookAt(spawnTarget(state.seed), glm::vec3(0, 1, 0));
        }
        if (capture) {
            camera->Update(std::min(delta, std::chrono::microseconds(100000)), *input, false, false, false);
            auto position = camera->mWorld.GetTranslation();
            position.x = std::clamp(position.x, -CoordinateLimit * ChunkSpan + ChunkSpan, CoordinateLimit * ChunkSpan - ChunkSpan);
            position.z = std::clamp(position.z, -CoordinateLimit * ChunkSpan + ChunkSpan, CoordinateLimit * ChunkSpan - ChunkSpan);
            position.y = std::clamp(position.y, MinChunkY * ChunkSpan + VoxelScale, (MaxChunkY + 1) * ChunkSpan - VoxelScale);
            camera->mWorld.SetTranslate(position);
        }
        if (initialized) {
            auto key = keyAt(camera->mWorld.GetTranslation());
            if (!center || *center != key) retarget(key);
        }
        uploadedBytes = 0;
        int integrated = 0;
        while (integrated < FrameChunkLimit) {
            auto reply = stream->poll();
            if (!reply) break;
            for (auto& chunk : reply->chunks) {
                if (!reply->edit) pending.erase(chunk.key);
                if ((reply->edit && resident.contains(chunk.key)) || targets.contains(chunk.key)) ingest(chunk);
            }
            integrated += reply->edit ? FrameChunkLimit : 1;
        }
        state = stream->status();
        if (closing && (state.error.empty() || retainRecoveryOnClose) && !state.editing) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            return;
        }
        if (initialized && state.error.empty() && !closing) {
            for (auto key : orderedTargets) {
                if (resident.contains(key) || pending.contains(key)) continue;
                if (!stream->request(key, epoch)) break;
                pending.insert(key);
            }
            if (!paused && !state.editing) {
                editCooldown -= std::chrono::duration<float>(delta).count();
                if (editCooldown <= 0 && (held(GLFW_KEY_SPACE) || mouseHeld(GLFW_MOUSE_BUTTON_LEFT) ||
                                         held(GLFW_KEY_X) || mouseHeld(GLFW_MOUSE_BUTTON_RIGHT))) {
                    bool add = held(GLFW_KEY_X) || mouseHeld(GLFW_MOUSE_BUTTON_RIGHT);
                    auto hit = pick();
                    if (!hit.miss && glm::distance(hit.world_pos, camera->mWorld.GetTranslation()) <= 16.0f) {
                        if (stream->edit({hit.world_pos, brushRadius, std::uint8_t(add ? Stone : Air)})) editCooldown = 0.15f;
                    }
                }
            }
        }
    }

    void render(bool showBasis, float basisLength, float basisWidth, float dt) {
        auto clip = camera->GetWorldToClipMatrix();
        auto position = camera->mWorld.GetTranslation();
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glUseProgram(skyProgram);
        auto inverse = glm::inverse(clip);
        glUniformMatrix4fv(glGetUniformLocation(skyProgram, "clip_to_world"), 1, GL_FALSE, glm::value_ptr(inverse));
        glUniform3fv(glGetUniformLocation(skyProgram, "camera_position"), 1, glm::value_ptr(position));
        glBindVertexArray(skyVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glUseProgram(program);
        glUniform3fv(glGetUniformLocation(program, "colorPalette"), palette.size(), glm::value_ptr(palette[0]));
        glUniform1f(glGetUniformLocation(program, "world_fog_radius"), FogDistance);
        drawn = 0;
        for (auto& [key, volume] : resident) {
            if (!volume || !visible(key, clip)) continue;
            volume->setAcceleration(acceleration);
            volume->render(glm::mat4(1), clip, position, showBasis, basisLength, basisWidth);
            ++drawn;
        }
        auto state = stream->status();
        ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.82f);
        ImGui::Begin("Floating islands", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::Text("WASD fly | Shift sprint | Space carve | X build | Esc menu");
        ImGui::Text("Seed %llu | %.1f, %.1f, %.1f", static_cast<unsigned long long>(state.seed), position.x, position.y, position.z);
        ImGui::Text("%.2f ms/frame | %zu/%d chunks | %d drawn", dt, resident.size(), ResidentLimit, drawn);
        ImGui::Text("Load queue %zu/8 | cache %.2f/64 MiB (%zu/2048 chunks)",
            state.queued, double(state.cachedBytes) / (1024 * 1024), state.cachedChunks);
        ImGui::Text("Voxel upload this frame: %zu bytes", uploadedBytes);
        if (!state.ready) ImGui::Text("Opening world...");
        else if (resident.size() < targets.size()) ImGui::Text("Loading nearby chunks: %zu/%zu", resident.size(), targets.size());
        if (state.editing) ImGui::Text("Saving brush before applying...");
        if (closing) ImGui::Text("Closing after pending save completes...");
        if (!state.error.empty()) {
            paused = true;
            ImGui::TextWrapped("World stopped: %s", state.error.c_str());
            ImGui::TextWrapped("An interrupted brush may complete on reopening. Retry recovery, or close while keeping all recovery files.");
            if (ImGui::Button("Retry world recovery")) restart();
            if (ImGui::Button("Close, keep recovery data")) {
                retainRecoveryOnClose = true;
                closing = true;
            }
        }
        if (paused) {
            ImGui::SliderFloat("Brush radius", &brushRadius, 0.25f, 2.0f, "%.2f");
            ImGui::Checkbox("Empty-space skipping", &acceleration);
            if (ImGui::Button("Return to spawn") && state.ready) {
                camera->mWorld.SetTranslate(spawnPosition(state.seed));
                camera->mWorld.LookAt(spawnTarget(state.seed), glm::vec3(0, 1, 0));
            }
            ImGui::SameLine();
            if (ImGui::Button("Close world")) closing = true;
        }
        ImGui::End();
    }

private:
    GLFWwindow* window;
    FPSCameraf* camera;
    InputHandler* input;
    std::filesystem::path path;
    std::optional<std::uint64_t> requestedSeed;
    std::unique_ptr<Stream> stream;
    std::array<glm::vec3, 256> palette;
    std::map<ChunkKey, std::unique_ptr<VoxelVolume>> resident;
    std::set<ChunkKey> targets, pending;
    std::vector<ChunkKey> orderedTargets;
    std::optional<ChunkKey> center;
    std::uint64_t epoch = 0;
    GLuint program = 0, skyProgram = 0, skyVao = 0;
    bool initialized = false, paused = false, closing = false, acceleration = true;
    bool retainRecoveryOnClose = false;
    bool cursorCaptured = false;
    float brushRadius = 0.75f, editCooldown = 0;
    std::size_t uploadedBytes = 0;
    int drawn = 0;

    bool pressed(int key) const { return (input->GetKeycodeState(key) & JUST_PRESSED) != 0; }
    bool held(int key) const { return (input->GetKeycodeState(key) & PRESSED) != 0; }
    bool mouseHeld(int button) const { return (input->GetMouseState(button) & PRESSED) != 0; }
    static std::int64_t distanceSquared(ChunkKey a, ChunkKey b) {
        std::int64_t x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
        return x*x + y*y + z*z;
    }
    void retarget(ChunkKey key) {
        center = key;
        ++epoch;
        stream->retarget(epoch);
        pending.clear();
        targets.clear();
        orderedTargets.clear();
        for (int z = -LoadRadius; z <= LoadRadius; ++z)
        for (int y = -LoadRadius; y <= LoadRadius; ++y)
        for (int x = -LoadRadius; x <= LoadRadius; ++x) {
            ChunkKey target{key.x+x, key.y+y, key.z+z};
            if (x*x+y*y+z*z <= LoadRadius * LoadRadius && valid(target)) {
                targets.insert(target);
                orderedTargets.push_back(target);
            }
        }
        std::sort(orderedTargets.begin(), orderedTargets.end(), [key](ChunkKey a, ChunkKey b) {
            auto da = distanceSquared(a, key), db = distanceSquared(b, key);
            return da == db ? a < b : da < db;
        });
        for (auto it = resident.begin(); it != resident.end();) {
            if (distanceSquared(it->first, key) > RetainRadius * RetainRadius) it = resident.erase(it);
            else ++it;
        }
    }
    void ingest(Chunk const& chunk) {
        if (!resident.contains(chunk.key) && resident.size() == ResidentLimit) {
            auto victim = resident.end();
            for (auto it = resident.begin(); it != resident.end(); ++it)
                if (!targets.contains(it->first) && (victim == resident.end() ||
                    distanceSquared(it->first, *center) > distanceSquared(victim->first, *center))) victim = it;
            if (victim == resident.end()) throw std::runtime_error("World resident budget invariant failed");
            resident.erase(victim);
        }
        auto& volume = resident[chunk.key];
        bool empty = std::all_of(chunk.data.begin(), chunk.data.end(), [](auto value) { return value == 0; });
        if (empty) {
            volume.reset();
            return;
        }
        if (!volume) {
            volume = std::make_unique<VoxelVolume>(ChunkSize, ChunkSize, ChunkSize,
                Transform().translate(origin(chunk.key)).scale(ChunkSpan));
            volume->setProgram(program);
            volume->setWorldStyle(true);
        }
        volume->setData(chunk.data);
        uploadedBytes += volume->upload();
    }
    static bool visible(ChunkKey key, glm::mat4 const& clip) {
        auto lo = origin(key);
        std::array<glm::vec4, 8> corners;
        for (int i = 0; i < 8; ++i)
            corners[i] = clip * glm::vec4(lo + glm::vec3(i&1 ? ChunkSpan : 0, i&2 ? ChunkSpan : 0, i&4 ? ChunkSpan : 0), 1);
        for (int axis = 0; axis < 3; ++axis) for (int side : {-1, 1}) {
            bool outside = true;
            for (auto corner : corners) if (side * corner[axis] <= corner.w) { outside = false; break; }
            if (outside) return false;
        }
        return true;
    }
    VoxelVolume::voxel_hit_t pick() {
        VoxelVolume::voxel_hit_t closest{.miss = true};
        auto position = camera->mWorld.GetTranslation();
        float distance = 16.0f * 16.0f;
        for (auto const& [key, volume] : resident) {
            if (!volume) continue;
            auto hit = volume->raycast(position, camera->mWorld.GetFront());
            if (!hit.miss && glm::length2(hit.world_pos - position) < distance) {
                closest = hit;
                distance = glm::length2(hit.world_pos - position);
            }
        }
        return closest;
    }
    void restart() {
        stream.reset();
        resident.clear();
        pending.clear();
        targets.clear();
        orderedTargets.clear();
        center.reset();
        initialized = false;
        epoch = 0;
        stream = std::make_unique<Stream>(path, requestedSeed);
    }
};
}
