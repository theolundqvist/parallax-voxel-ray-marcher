#pragma once

#include "BrickPool.hpp"
#include "core/ShaderProgramManager.hpp"
#include <array>

namespace world {
// All positions are metres relative to the camera's level-0 anchor chunk corner.
struct FrameUniforms {
    glm::mat4 worldToClip;
    glm::mat4 clipToWorld;
    glm::vec3 cameraPosition;
    glm::vec3 sunDirection;
    bool acceleration;
    std::array<glm::vec3, 256> const* palette;
    int width, height;
};

struct LevelUniforms {
    int level;
    glm::vec3 regionOrigin;
    float chunkSpan;
    glm::ivec3 regionSize;
    glm::ivec3 pageOrigin;
    glm::ivec3 holeLo, holeHi;
    // Highest region-local row with a non-zero table entry; -1 draws nothing at this level.
    int topRow;
};

// beginFrame resets levels; drawLevel records disjoint drawn domains. march merges their chunks
// by ray distance into RGB8 opaque colour and RGBA32F (opaque depth, water length, first water
// boundary depth, signed face code) targets, left bound for readback. composite adds sky/fog/water.
class WorldRenderer {
public:
    WorldRenderer() = default;
    WorldRenderer(WorldRenderer const&) = delete;
    WorldRenderer& operator=(WorldRenderer const&) = delete;
    ~WorldRenderer();

    void init(ShaderProgramManager& programs);
    void refreshPrograms();
    void resize(int width, int height);
    void beginFrame(FrameUniforms const& frame);
    void drawLevel(LevelUniforms const& level);
    void march();
    void composite();
    BrickPool& bricks() { return pool; }
    LevelTable& table(int level) { return tables[level]; }

private:
    // std140 mirror of world.frag's level_t: sizeDrawn.w is the drawn flag, pageOrigin.w the top row.
    struct LevelBlock {
        glm::vec4 originSpan;
        glm::ivec4 sizeDrawn;
        glm::ivec4 pageOrigin;
        glm::ivec4 holeLo;
        glm::ivec4 holeHi;
    };
    struct WorldUniforms {
        GLint volume, occupancy, pageTables, acceleration, clipToWorld, camera, sun, palette;
        GLuint levels;
    };
    struct CompositeUniforms {
        GLint color, distance, clipToWorld, camera, sun;
    };

    GLuint worldProgram = 0, compositeProgram = 0;
    WorldUniforms world{};
    CompositeUniforms comp{};
    GLuint emptyVao = 0, levelBuffer = 0;
    GLuint framebuffer = 0, colorTexture = 0, distanceTexture = 0;
    int width = 0, height = 0;
    GLint targetFramebuffer = 0;
    FrameUniforms current{};
    std::array<LevelBlock, LevelCount> levels{};
    BrickPool pool;
    LevelAtlas atlas;
    std::array<LevelTable, LevelCount> tables;
};
}
