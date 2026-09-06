#pragma once

#include "Frontier.hpp"
#include "Generate.hpp"
#include "Stream.hpp"
#include "WorldRenderer.hpp"
#include "core/FPSCamera.h"
#include "core/InputHandler.h"
#include "core/ShaderProgramManager.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <glm/gtx/norm.hpp>
#include <map>
#include <set>

namespace world {
class WorldApp {
public:
    WorldApp(GLFWwindow* window, FPSCameraf* camera, InputHandler* input,
             ShaderProgramManager* programs, std::filesystem::path path,
             std::optional<std::uint64_t> seed)
        : window(window), camera(camera), input(input), path(std::move(path)), requestedSeed(seed),
          stream(std::make_unique<Stream>(this->path, seed)), palette(worldPalette()) {
        renderer.init(*programs);
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        resize(width, height);
        camera->SetProjection(glm::radians(65.0f), float(width) / float(height), 0.05f, 40000.0f);
        camera->mMovementSpeed = glm::vec3(3.0f);
        teleport(spawnPosition(seed.value_or(DefaultSeed)), spawnTarget(seed.value_or(DefaultSeed)));
    }
    ~WorldApp() {
        stream.reset();
        resident.clear();
    }

    void refreshPrograms() { renderer.refreshPrograms(); }

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
            teleport(spawnPosition(state.seed), spawnTarget(state.seed));
        }
        if (capture) {
            camera->Update(std::min(delta, std::chrono::microseconds(100000)), *input, false, false, false);
            move(glm::dvec3(camera->mWorld.GetTranslation()));
        }
        if (initialized && (!center || *center != position.anchor)) retarget();
        uploadedBytes = 0;
        while (uploadedBytes < FrameUploadBudget) {
            auto reply = stream->poll();
            if (!reply) break;
            for (auto& chunk : reply->chunks) {
                if (reply->edit) {
                    if (resident.contains(chunk.key)) {
                        ingest(chunk);
                        for (int level = 1; level <= OverlayLevels; ++level) {
                            auto ancestor = keyAtLevel(chunk.key, level);
                            if (auto it = ancestor ? resident.find(*ancestor) : resident.end(); it != resident.end())
                                it->second.stale = true;
                        }
                    }
                    continue;
                }
                pending.erase(chunk.key);
                if (targetSet.contains(chunk.key)) ingest(chunk);
            }
        }
        state = stream->status();
        if (closing && (state.error.empty() || retainRecoveryOnClose) && !state.editing) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            return;
        }
        if (initialized && state.error.empty() && !closing) {
            for (auto key : targets) {
                if (pending.contains(key) || deferred.contains(key)) continue;
                if (auto it = resident.find(key); it != resident.end() && !it->second.stale) continue;
                if (!stream->request(key, epoch)) break;
                pending.insert(key);
            }
            if (!paused && !state.editing) {
                editCooldown -= std::chrono::duration<float>(delta).count();
                if (editCooldown <= 0 && (held(GLFW_KEY_SPACE) || mouseHeld(GLFW_MOUSE_BUTTON_LEFT) ||
                                         held(GLFW_KEY_X) || mouseHeld(GLFW_MOUSE_BUTTON_RIGHT))) {
                    bool add = held(GLFW_KEY_X) || mouseHeld(GLFW_MOUSE_BUTTON_RIGHT);
                    if (auto hit = pick()) {
                        auto centre = normalizedPosition(position.anchor, *hit);
                        if (centre && stream->edit({*centre, brushRadius, std::uint8_t(add ? Stone : Air)}))
                            editCooldown = 0.15f;
                    }
                }
            }
        }
        if (drawnDirty) refreshDrawn();
    }

    void resize(int width, int height) {
        if (width == viewWidth && height == viewHeight) return;
        viewWidth = width;
        viewHeight = height;
        camera->SetAspect(float(width) / float(std::max(height, 1)));
    }

    void render(float dt) {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        resize(width, height);
        auto cameraPosition = camera->mWorld.GetTranslation();
        FrameUniforms frame{
            .worldToClip = camera->GetWorldToClipMatrix(),
            .clipToWorld = camera->GetClipToWorldMatrix(),
            .cameraPosition = cameraPosition,
            .seaLevel = float(SeaLevel - double(position.anchor.y) * double(ChunkSpan)),
            .sunDirection = glm::normalize(glm::vec3(0.35f, 0.8f, 0.45f)),
            .acceleration = acceleration,
            .palette = &palette,
            .width = width,
            .height = height,
        };
        renderer.beginFrame(frame);
        for (int level = 0; level < LevelCount; ++level) {
            auto r = region(position.anchor, level, ShellRadius);
            if (!r) continue;
            auto origin = relativeOrigin(r->lo, position.anchor, MaxChunkDelta);
            if (!origin) continue;
            LevelUniforms uniforms{
                .level = level,
                .regionOrigin = *origin,
                .chunkSpan = spanAt(level),
                .regionSize = glm::ivec3(int(r->hi.x - r->lo.x + 1), int(r->hi.y - r->lo.y + 1), int(r->hi.z - r->lo.z + 1)),
                .pageOrigin = glm::ivec3(texel(r->lo.x), texel(r->lo.y), texel(r->lo.z)),
                .holeLo = glm::ivec3(0),
                .holeHi = glm::ivec3(0),
            };
            if (holeValid[level]) {
                auto finer = *region(position.anchor, level - 1, ShellRadius);
                uniforms.holeLo = glm::ivec3(int(finer.lo.x / 2 - r->lo.x), int(finer.lo.y / 2 - r->lo.y), int(finer.lo.z / 2 - r->lo.z));
                uniforms.holeHi = glm::ivec3(int((finer.hi.x + 1) / 2 - r->lo.x), int((finer.hi.y + 1) / 2 - r->lo.y), int((finer.hi.z + 1) / 2 - r->lo.z));
            }
            renderer.drawLevel(uniforms);
        }
        renderer.march();
        renderer.composite();

        auto state = stream->status();
        auto worldY = double(position.anchor.y) * double(ChunkSpan) + position.offset.y;
        ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.82f);
        ImGui::Begin("Mountains", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::Text("WASD fly | Shift sprint | Space carve | X build | Esc menu");
        ImGui::Text("Seed %llu | chunk %lld, %lld, %lld | altitude %.1f m",
            static_cast<unsigned long long>(state.seed), static_cast<long long>(position.anchor.x),
            static_cast<long long>(position.anchor.y), static_cast<long long>(position.anchor.z), worldY);
        ImGui::Text("%.2f ms/frame | %zu resident | %d/%d bricks | %zu drawn", dt, resident.size(),
            renderer.bricks().used(), BrickCapacity, drawn.size());
        ImGui::Text("Load queue %zu | cache %.2f/64 MiB (%zu/2048 chunks) | upload %zu bytes",
            state.queued, double(state.cachedBytes) / (1024 * 1024), state.cachedChunks, uploadedBytes);
        if (!state.ready) ImGui::Text("Opening world...");
        else if (pending.size()) ImGui::Text("Streaming %zu chunks", pending.size());
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
            if (ImGui::Button("Return to spawn") && state.ready) teleport(spawnPosition(state.seed), spawnTarget(state.seed));
            ImGui::SameLine();
            if (ImGui::Button("Close world")) closing = true;
        }
        ImGui::End();
    }

private:
    static constexpr int MaxChunkDelta = 1 << 24;
    static constexpr float ReachMetres = 16.0f;
    struct Resident {
        std::uint8_t uniform = Air;
        std::unique_ptr<ChunkData> cpu;
        std::optional<std::uint16_t> slot;
        bool stale = false;
    };

    GLFWwindow* window;
    FPSCameraf* camera;
    InputHandler* input;
    std::filesystem::path path;
    std::optional<std::uint64_t> requestedSeed;
    std::unique_ptr<Stream> stream;
    std::array<glm::vec3, 256> palette;
    WorldRenderer renderer;
    WorldPosition position{{0, 0, 0, 0}, glm::dvec3(0)};
    std::map<ChunkKey, Resident> resident;
    std::vector<ChunkKey> targets, drawn;
    std::set<ChunkKey> targetSet, pending, deferred, drawnSet;
    std::array<bool, LevelCount> holeValid{};
    std::optional<ChunkKey> center;
    std::uint64_t epoch = 0;
    int viewWidth = 0, viewHeight = 0;
    bool initialized = false, paused = false, closing = false, acceleration = true;
    bool retainRecoveryOnClose = false, cursorCaptured = false, drawnDirty = true;
    float brushRadius = 0.75f, editCooldown = 0;
    std::size_t uploadedBytes = 0;

    bool pressed(int key) const { return (input->GetKeycodeState(key) & JUST_PRESSED) != 0; }
    bool held(int key) const { return (input->GetKeycodeState(key) & PRESSED) != 0; }
    bool mouseHeld(int button) const { return (input->GetMouseState(button) & PRESSED) != 0; }
    static int texel(std::int64_t v) { return int(((v % PageSize) + PageSize) % PageSize); }

    // The camera's float translation is always its offset inside the anchor chunk, so precision never drifts.
    void move(glm::dvec3 offset) {
        auto next = normalizedPosition(position.anchor, offset);
        if (next) {
            auto worldY = double(next->anchor.y) * double(ChunkSpan) + next->offset.y;
            auto clamped = glm::clamp(worldY, double(TerrainFloor), double(TravelCeiling));
            if (clamped != worldY) next = normalizedPosition(next->anchor, {next->offset.x, next->offset.y + (clamped - worldY), next->offset.z});
        }
        if (next) position = *next;
        camera->mWorld.SetTranslate(glm::vec3(position.offset));
    }
    void teleport(WorldPosition where, WorldPosition target) {
        position = where;
        camera->mWorld.SetTranslate(glm::vec3(position.offset));
        auto relative = relativeOrigin(target.anchor, position.anchor, MaxChunkDelta);
        if (relative) camera->mWorld.LookAt(*relative + glm::vec3(target.offset), glm::vec3(0, 1, 0));
    }
    void retarget() {
        center = position.anchor;
        ++epoch;
        stream->retarget(epoch);
        pending.clear();
        deferred.clear();
        targets = residencyTargets(position.anchor, ShellRadius);
        targetSet = std::set<ChunkKey>(targets.begin(), targets.end());
        for (auto it = resident.begin(); it != resident.end();) {
            if (retained(it->first, position.anchor, ShellRadius)) { ++it; continue; }
            if (it->second.slot) renderer.bricks().release(*it->second.slot);
            it = resident.erase(it);
        }
        drawnDirty = true;
    }
    void ingest(Chunk& chunk) {
        Resident* entry = nullptr;
        if (auto it = resident.find(chunk.key); it != resident.end()) entry = &it->second;
        if (chunk.data) {
            std::optional<std::uint16_t> slot = entry ? entry->slot : std::nullopt;
            if (!slot) slot = renderer.bricks().allocate();
            if (!slot) slot = reclaimBrick();
            if (!slot) {
                deferred.insert(chunk.key);
                return;
            }
            uploadedBytes += renderer.bricks().upload(*slot, *chunk.data);
            auto& r = entry ? *entry : resident[chunk.key];
            r.slot = slot;
            r.uniform = Air;
            r.cpu = chunk.key.level == 0 ? std::move(chunk.data) : nullptr;
            r.stale = false;
        } else {
            auto& r = entry ? *entry : resident[chunk.key];
            if (r.slot) renderer.bricks().release(*r.slot);
            r.slot.reset();
            r.cpu.reset();
            r.uniform = chunk.uniform;
            r.stale = false;
        }
        drawnDirty = true;
    }
    // Only hysteresis-retained bricks are reclaimable; a finer chunk that cannot get a slot stays covered by its parent.
    std::optional<std::uint16_t> reclaimBrick() {
        for (auto it = resident.begin(); it != resident.end(); ++it) {
            if (!it->second.slot || targetSet.contains(it->first)) continue;
            auto slot = *it->second.slot;
            resident.erase(it);
            drawnDirty = true;
            return slot;
        }
        return std::nullopt;
    }
    std::uint16_t entryOf(Resident const& r) const {
        if (r.slot) return std::uint16_t(256 + *r.slot);
        return r.uniform == Air ? 0 : r.uniform;
    }
    void refreshDrawn() {
        drawnDirty = false;
        auto isResident = [this](ChunkKey k) { return resident.contains(k); };
        drawn = world::drawnSet(position.anchor, ShellRadius, isResident);
        std::set<ChunkKey> next(drawn.begin(), drawn.end());
        for (auto key : drawnSet)
            if (!next.contains(key)) renderer.table(key.level).set(key, 0);
        for (auto key : drawn) renderer.table(key.level).set(key, entryOf(resident.at(key)));
        drawnSet = std::move(next);
        holeValid.fill(true);
        holeValid[0] = false;
        for (auto key : drawn) {
            if (key.level == 0) continue;
            auto finer = region(position.anchor, key.level - 1, ShellRadius);
            auto first = childOf(key, 0), last = childOf(key, 7);
            if (!finer || !first || !last) continue;
            if (finer->contains(*first) && finer->contains(*last)) holeValid[key.level] = false;
        }
    }
    std::uint8_t voxelAt(glm::ivec3 v) const {
        auto key = offsetKey(position.anchor, glm::ivec3(glm::floor(glm::vec3(v) / float(ChunkSize))));
        if (!key) return Bedrock;
        auto it = resident.find(*key);
        if (it == resident.end() || it->first.level != 0) return Air;
        if (!it->second.cpu) return it->second.uniform;
        auto local = v - glm::ivec3(glm::floor(glm::vec3(v) / float(ChunkSize))) * ChunkSize;
        return (*it->second.cpu)[index(local.x, local.y, local.z)] & MaterialMask;
    }
    // Amanatides-Woo walk over resident level-0 chunks within reach; returns the hit point in render space.
    std::optional<glm::dvec3> pick() const {
        auto origin = glm::dvec3(position.offset);
        auto direction = glm::dvec3(glm::normalize(camera->mWorld.GetFront()));
        auto p = origin / double(VoxelScale);
        glm::ivec3 cell(glm::floor(p));
        glm::ivec3 step;
        glm::dvec3 tMax, tDelta;
        for (int axis = 0; axis < 3; ++axis) {
            step[axis] = direction[axis] > 0 ? 1 : direction[axis] < 0 ? -1 : 0;
            tDelta[axis] = step[axis] ? std::abs(1.0 / direction[axis]) : std::numeric_limits<double>::infinity();
            double boundary = direction[axis] > 0 ? cell[axis] + 1.0 : double(cell[axis]);
            tMax[axis] = step[axis] ? (boundary - p[axis]) / direction[axis] : std::numeric_limits<double>::infinity();
        }
        double limit = ReachMetres / VoxelScale;
        double t = 0;
        while (t <= limit) {
            if (voxelAt(cell) != Air) return origin + direction * (t * double(VoxelScale));
            int axis = tMax.x < tMax.y ? (tMax.x < tMax.z ? 0 : 2) : (tMax.y < tMax.z ? 1 : 2);
            t = tMax[axis];
            tMax[axis] += tDelta[axis];
            cell[axis] += step[axis];
        }
        return std::nullopt;
    }
    void restart() {
        stream.reset();
        for (auto& [key, r] : resident) if (r.slot) renderer.bricks().release(*r.slot);
        resident.clear();
        pending.clear();
        deferred.clear();
        targets.clear();
        targetSet.clear();
        center.reset();
        initialized = false;
        epoch = 0;
        drawnDirty = true;
        stream = std::make_unique<Stream>(path, requestedSeed);
    }
};
}
