#pragma once

#include <array>
#include <cmath>
#include <compare>
#include <cstdint>
#include <stdexcept>
#include <glm/glm.hpp>

namespace world {
inline constexpr int ChunkSize = 32;
inline constexpr float VoxelScale = 0.25f;
inline constexpr float ChunkSpan = ChunkSize * VoxelScale;
inline constexpr int MinChunkY = -4;
inline constexpr int MaxChunkY = 5;
inline constexpr int CoordinateLimit = 16384;
inline constexpr std::uint32_t GeneratorVersion = 1;
inline constexpr std::uint64_t DefaultSeed = 20260905;
inline constexpr int LoadRadius = 5;
inline constexpr int RetainRadius = 6;
inline constexpr int ResidentLimit = 640;
inline constexpr int FrameChunkLimit = 4;
inline constexpr float FogDistance = 26.0f;

enum Material : std::uint8_t { Air, Grass, Stone, Soil, Sand, Crystal };
struct ChunkKey {
    std::int32_t x, y, z;
    auto operator<=>(ChunkKey const&) const = default;
};
using ChunkData = std::array<std::uint8_t, ChunkSize * ChunkSize * ChunkSize>;
struct Chunk {
    ChunkKey key;
    ChunkData data;
};
inline int index(int x, int y, int z) { return x + ChunkSize * (y + ChunkSize * z); }
inline bool valid(ChunkKey key) {
    return key.x >= -CoordinateLimit && key.x < CoordinateLimit &&
           key.z >= -CoordinateLimit && key.z < CoordinateLimit &&
           key.y >= MinChunkY && key.y <= MaxChunkY;
}
inline glm::vec3 origin(ChunkKey key) {
    return glm::vec3(key.x, key.y, key.z) * ChunkSpan;
}
inline ChunkKey keyAt(glm::vec3 position) {
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) ||
        glm::any(glm::lessThan(position, glm::vec3(-CoordinateLimit * ChunkSpan))) ||
        glm::any(glm::greaterThanEqual(position, glm::vec3(CoordinateLimit * ChunkSpan))))
        throw std::runtime_error("Position outside supported world coordinates");
    auto p = glm::ivec3(glm::floor(position / ChunkSpan));
    return {p.x, p.y, p.z};
}
struct Brush {
    glm::vec3 center;
    float radius;
    std::uint8_t material;
};
}
