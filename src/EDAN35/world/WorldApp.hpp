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
#include <algorithm>
#include <array>
#include <cmath>
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
        camera->mMovementSpeed = glm::vec3(flightSpeed);
        glGenQueries(int(gpuQueries.size()), gpuQueries.data());
        teleport(spawnPosition(seed.value_or(DefaultSeed)), spawnTarget(seed.value_or(DefaultSeed)));
    }
    ~WorldApp() {
        glDeleteQueries(int(gpuQueries.size()), gpuQueries.data());
        stream.reset();
        resident.clear();
    }

    void refreshPrograms() { renderer.refreshPrograms(); }

    void recordFrame(float cpuMs, float presentMs) {
        stats.cpu.add(cpuMs);
        stats.present.add(presentMs);
        stats.frame.add(cpuMs + presentMs);
    }

    void update(std::chrono::microseconds delta) {
        auto start = std::chrono::steady_clock::now();
        step(delta);
        stats.update.add(millisecondsSince(start));
    }

    void step(std::chrono::microseconds delta) {
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
            auto& io = ImGui::GetIO();
            if (io.MouseWheel != 0 && !io.WantCaptureMouse) {
                flightSpeed = glm::clamp(flightSpeed * std::pow(1.25f, io.MouseWheel), 1.0f, 500.0f);
                camera->mMovementSpeed = glm::vec3(flightSpeed);
            }
            camera->Update(delta, *input, false, false, false);
            move(glm::dvec3(camera->mWorld.GetTranslation()));
        }
        if (initialized && (!center || *center != position.anchor)) retarget();
        uploadedBytes = 0;
        tableWrites = 0;
        constexpr std::size_t FrameReplyBudget = 64;
        for (std::size_t replies = 0; replies < FrameReplyBudget && uploadedBytes < FrameUploadBudget; ++replies) {
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
                if (!stream->request(key)) break;
                pending.insert(key);
            }
            if (!paused && !state.editing) {
                if (held(GLFW_KEY_SPACE) || mouseHeld(GLFW_MOUSE_BUTTON_LEFT) ||
                    held(GLFW_KEY_X) || mouseHeld(GLFW_MOUSE_BUTTON_RIGHT)) {
                    bool add = held(GLFW_KEY_X) || mouseHeld(GLFW_MOUSE_BUTTON_RIGHT);
                    if (auto hit = pick()) {
                        auto centre = normalizedPosition(position.anchor, *hit);
                        if (centre) stream->edit({*centre, brushRadius, std::uint8_t(add ? Stone : Air)});
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

    void render() {
        auto start = std::chrono::steady_clock::now();
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        resize(width, height);
        while (gpuPending != 0) {
            auto query = gpuQueries[gpuRead];
            GLint ready = 0;
            glGetQueryObjectiv(query, GL_QUERY_RESULT_AVAILABLE, &ready);
            if (!ready) break;
            GLuint64 nanoseconds = 0;
            glGetQueryObjectui64v(query, GL_QUERY_RESULT, &nanoseconds);
            stats.gpu.add(float(nanoseconds) * 1e-6f);
            gpuRead = (gpuRead + 1) % gpuQueries.size();
            --gpuPending;
        }
        bool measureGpu = gpuPending < gpuQueries.size();
        if (measureGpu)
            glBeginQuery(GL_TIME_ELAPSED, gpuQueries[(gpuRead + gpuPending) % gpuQueries.size()]);
        auto cameraPosition = camera->mWorld.GetTranslation();
        FrameUniforms frame{
            .worldToClip = camera->GetWorldToClipMatrix(),
            .clipToWorld = camera->GetClipToWorldMatrix(),
            .cameraPosition = cameraPosition,
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
                .topRow = topRow[level],
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
        if (measureGpu) {
            glEndQuery(GL_TIME_ELAPSED);
            ++gpuPending;
        }
        stats.submit.add(millisecondsSince(start));

        auto state = stream->status();
        auto worldY = double(position.anchor.y) * double(ChunkSpan) + position.offset.y;
        ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.82f);
        ImGui::SetNextWindowSize(ImVec2(std::min(570.0f, ImGui::GetIO().DisplaySize.x - 32.0f), 0));
        auto flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
        if (!paused) flags |= ImGuiWindowFlags_NoInputs;
        ImGui::Begin("Mountains", nullptr, flags);
        ImGui::Text("Frame %.1f ms avg / %.1f p99 | %.0f fps | %d x %d pixels",
            stats.frame.mean(), stats.frame.p99(),
            stats.frame.mean() > 0 ? 1000.0f / stats.frame.mean() : 0.0f, width, height);
        ImGui::Text("CPU + driver %.1f / %.1f ms | Present %.1f / %.1f ms",
            stats.cpu.mean(), stats.cpu.p99(), stats.present.mean(), stats.present.p99());
        if (stats.gpu.count)
            ImGui::Text("GPU world %.1f / %.1f ms | %zu queries pending",
                stats.gpu.mean(), stats.gpu.p99(), gpuPending);
        else ImGui::TextUnformatted("GPU world: waiting for first completed frame");
        ImGui::Text("CPU update %.1f ms | World submission %.1f ms",
            stats.update.mean(), stats.submit.mean());
        ImGui::TextWrapped("Average / p99 over 120 samples. CPU includes driver waits, excludes present. GPU overlaps CPU; do not add them.");
        ImGui::Separator();
        ImGui::Text("Flight %.0f m/s | Shift %.0f m/s | Ctrl %.1f m/s",
            flightSpeed, flightSpeed * 4, flightSpeed * 0.25f);
        ImGui::TextWrapped("Mouse: look | WASD: fly | Q/E: descend/ascend");
        ImGui::TextWrapped("Left Shift: 4x | Left Ctrl: 0.25x | Wheel: change speed");
        ImGui::TextWrapped("Space / left click: carve | X / right click: build");
        ImGui::TextWrapped("Esc: menu | R: reload shaders | F2: HUD | F3: logs");
        ImGui::TextWrapped("F11: fullscreen | B: axes | M: wireframe");
        ImGui::Separator();
        ImGui::Text("Seed %llu | chunk %lld, %lld, %lld | altitude %.1f m",
            static_cast<unsigned long long>(state.seed), static_cast<long long>(position.anchor.x),
            static_cast<long long>(position.anchor.y), static_cast<long long>(position.anchor.z), worldY);
        ImGui::Text("%zu resident | %d/%d bricks | %zu drawn | %d page-table writes", resident.size(),
            renderer.bricks().used(), BrickCapacity, drawn.size(), tableWrites);
        ImGui::Text("Load queue %zu | Cache %.1f/64 MiB | Upload %.1f KiB",
            state.queued, double(state.cachedBytes) / (1024 * 1024), double(uploadedBytes) / 1024);
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
            if (ImGui::SliderFloat("Flight speed (m/s)", &flightSpeed, 1.0f, 500.0f, "%.0f", ImGuiSliderFlags_Logarithmic))
                camera->mMovementSpeed = glm::vec3(flightSpeed);
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
    struct Series {
        std::array<float, 120> samples{};
        int count = 0, next = 0;
        void add(float value) {
            samples[next] = value;
            next = (next + 1) % int(samples.size());
            count = std::min(count + 1, int(samples.size()));
        }
        float mean() const {
            float sum = 0;
            for (int i = 0; i < count; ++i) sum += samples[i];
            return count ? sum / float(count) : 0.0f;
        }
        float p99() const {
            if (!count) return 0;
            auto sorted = samples;
            auto index = (99 * count + 99) / 100 - 1;
            std::nth_element(sorted.begin(), sorted.begin() + index, sorted.begin() + count);
            return sorted[index];
        }
    };
    struct Stats {
        Series frame, cpu, present, update, submit, gpu;
    };
    static float millisecondsSince(std::chrono::steady_clock::time_point start) {
        return std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
    }
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
    std::set<ChunkKey> targetSet, pending, deferred;
    std::map<ChunkKey, std::uint16_t> drawnEntries;
    std::array<bool, LevelCount> holeValid{};
    std::array<int, LevelCount> topRow{};
    std::optional<ChunkKey> center;
    int viewWidth = 0, viewHeight = 0;
    bool initialized = false, paused = false, closing = false, acceleration = true;
    bool retainRecoveryOnClose = false, cursorCaptured = false, drawnDirty = true;
    float brushRadius = 0.75f, flightSpeed = 48.0f;
    std::size_t uploadedBytes = 0;
    int tableWrites = 0;
    Stats stats;
    std::array<GLuint, 4> gpuQueries{};
    std::size_t gpuRead = 0, gpuPending = 0;

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
        deferred.clear();
        targets = residencyTargets(position.anchor, ShellRadius);
        targetSet = std::set<ChunkKey>(targets.begin(), targets.end());
        stream->retarget(position.anchor);
        std::erase_if(pending, [this](ChunkKey key) { return !targetSet.contains(key); });
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
        for (auto it = drawnEntries.begin(); it != drawnEntries.end();) {
            if (next.contains(it->first)) { ++it; continue; }
            if (it->second != 0) {
                renderer.table(it->first.level).set(it->first, 0);
                ++tableWrites;
            }
            it = drawnEntries.erase(it);
        }
        topRow.fill(-1);
        for (auto key : drawn) {
            auto entry = entryOf(resident.at(key));
            auto it = drawnEntries.try_emplace(key, 0).first;
            if (it->second != entry) {
                renderer.table(key.level).set(key, entry);
                it->second = entry;
                ++tableWrites;
            }
            if (entry == 0) continue;
            auto r = region(position.anchor, key.level, ShellRadius);
            topRow[key.level] = std::max(topRow[key.level], int(key.y - r->lo.y));
        }
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
        drawnDirty = true;
        stream = std::make_unique<Stream>(path, requestedSeed);
    }
};
}
