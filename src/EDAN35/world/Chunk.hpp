#pragma once

#include <array>
#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <glm/glm.hpp>

namespace world {
inline constexpr int ChunkSize = 32;
inline constexpr float VoxelScale = 0.25f;
inline constexpr float ChunkSpan = ChunkSize * VoxelScale;
inline constexpr int LevelCount = 10;
inline constexpr int ShellRadius = 3;
inline constexpr int PageSize = 9;
inline constexpr int OverlayLevels = 3;
// Every resident chunk (10 levels x 8^3 shell) can hold a brick, so terrain that is non-uniform everywhere never exhausts the pool.
inline constexpr int BrickCapacity = 5120;
inline constexpr std::size_t FrameUploadBudget = std::size_t{1} << 20;
inline constexpr int MaxBrushChunks = 8;
inline constexpr std::uint32_t GeneratorVersion = 4;
inline constexpr std::uint64_t DefaultSeed = 20260905;

enum Material : std::uint8_t { Air, Grass, Stone, Soil, Sand, Crystal, Snow, Rock, Bedrock };
inline constexpr std::uint8_t MaterialMask = 0x0F;

struct ChunkKey {
    std::int64_t x, y, z;
    std::uint8_t level = 0;
    auto operator<=>(ChunkKey const&) const = default;
};
inline float spanAt(int level) { return ChunkSpan * float(1 << level); }
inline float voxelSizeAt(int level) { return VoxelScale * float(1 << level); }

using ChunkData = std::array<std::uint8_t, ChunkSize * ChunkSize * ChunkSize>;
struct Chunk {
    ChunkKey key;
    std::uint8_t uniform = Air;
    std::unique_ptr<ChunkData> data;
};
inline int index(int x, int y, int z) { return x + ChunkSize * (y + ChunkSize * z); }

// Every representable key is a legitimate key: failures below are machine bounds, never map edges.
inline std::optional<std::int64_t> checkedAdd(std::int64_t a, std::int64_t b) {
    constexpr auto max = std::numeric_limits<std::int64_t>::max();
    constexpr auto min = std::numeric_limits<std::int64_t>::min();
    if ((b > 0 && a > max - b) || (b < 0 && a < min - b)) return std::nullopt;
    return a + b;
}
inline std::optional<std::int64_t> checkedShiftLeft(std::int64_t value, int bits) {
    if (value > (std::numeric_limits<std::int64_t>::max() >> bits) ||
        value < (std::numeric_limits<std::int64_t>::min() >> bits))
        return std::nullopt;
    return value * (std::int64_t{1} << bits);
}
inline std::optional<std::int64_t> boundedDifference(std::int64_t value, std::int64_t base,
                                                     std::int64_t maxAbs) {
    bool negative = value < base;
    std::uint64_t magnitude = negative ? std::uint64_t(base) - std::uint64_t(value)
                                       : std::uint64_t(value) - std::uint64_t(base);
    if (magnitude > std::uint64_t(maxAbs)) return std::nullopt;
    return negative ? -std::int64_t(magnitude) : std::int64_t(magnitude);
}

inline std::optional<ChunkKey> offsetKey(ChunkKey base, glm::ivec3 delta) {
    auto x = checkedAdd(base.x, delta.x);
    auto y = checkedAdd(base.y, delta.y);
    auto z = checkedAdd(base.z, delta.z);
    if (!x || !y || !z) return std::nullopt;
    return ChunkKey{*x, *y, *z, base.level};
}
inline std::optional<glm::ivec3> boundedDelta(ChunkKey value, ChunkKey base, int maxAbs) {
    if (value.level != base.level) return std::nullopt;
    auto x = boundedDifference(value.x, base.x, maxAbs);
    auto y = boundedDifference(value.y, base.y, maxAbs);
    auto z = boundedDifference(value.z, base.z, maxAbs);
    if (!x || !y || !z) return std::nullopt;
    return glm::ivec3(int(*x), int(*y), int(*z));
}
inline std::optional<ChunkKey> parentOf(ChunkKey key) {
    if (key.level + 1 >= LevelCount) return std::nullopt;
    return ChunkKey{key.x >> 1, key.y >> 1, key.z >> 1, std::uint8_t(key.level + 1)};
}
inline std::optional<ChunkKey> childOf(ChunkKey key, int child) {
    if (key.level == 0) return std::nullopt;
    auto x = checkedShiftLeft(key.x, 1);
    auto y = checkedShiftLeft(key.y, 1);
    auto z = checkedShiftLeft(key.z, 1);
    if (!x || !y || !z) return std::nullopt;
    return ChunkKey{*x + (child & 1), *y + ((child >> 1) & 1), *z + ((child >> 2) & 1),
                    std::uint8_t(key.level - 1)};
}
inline std::optional<ChunkKey> levelZeroCorner(ChunkKey key) {
    auto x = checkedShiftLeft(key.x, key.level);
    auto y = checkedShiftLeft(key.y, key.level);
    auto z = checkedShiftLeft(key.z, key.level);
    if (!x || !y || !z) return std::nullopt;
    return ChunkKey{*x, *y, *z, 0};
}
inline std::optional<ChunkKey> keyAtLevel(ChunkKey levelZero, int level) {
    if (levelZero.level != 0 || level < 0 || level >= LevelCount) return std::nullopt;
    return ChunkKey{levelZero.x >> level, levelZero.y >> level, levelZero.z >> level,
                    std::uint8_t(level)};
}
// Metres from the render origin's corner to the key's corner; the origin is always a level-0 key.
inline std::optional<glm::vec3> relativeOrigin(ChunkKey key, ChunkKey renderOrigin, int maxChunkDelta) {
    auto corner = levelZeroCorner(key);
    if (!corner) return std::nullopt;
    auto delta = boundedDelta(*corner, renderOrigin, maxChunkDelta);
    if (!delta) return std::nullopt;
    return glm::vec3(*delta) * ChunkSpan;
}

struct WorldPosition {
    ChunkKey anchor;
    glm::dvec3 offset;
};
inline std::optional<WorldPosition> normalizedPosition(ChunkKey anchor, glm::dvec3 offset) {
    if (anchor.level != 0) return std::nullopt;
    constexpr double limit = 9223372036854775808.0;
    std::int64_t carry[3];
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(offset[axis])) return std::nullopt;
        double q = std::floor(offset[axis] / double(ChunkSpan));
        if (q < -limit || q >= limit) return std::nullopt;
        carry[axis] = std::int64_t(q);
    }
    auto x = checkedAdd(anchor.x, carry[0]);
    auto y = checkedAdd(anchor.y, carry[1]);
    auto z = checkedAdd(anchor.z, carry[2]);
    if (!x || !y || !z) return std::nullopt;
    glm::dvec3 local;
    for (int axis = 0; axis < 3; ++axis) {
        local[axis] = offset[axis] - double(carry[axis]) * double(ChunkSpan);
        local[axis] = glm::clamp(local[axis], 0.0, std::nextafter(double(ChunkSpan), 0.0));
    }
    return WorldPosition{ChunkKey{*x, *y, *z, 0}, local};
}
inline std::optional<WorldPosition> translated(WorldPosition position, glm::dvec3 deltaMetres) {
    return normalizedPosition(position.anchor, position.offset + deltaMetres);
}

struct Brush {
    WorldPosition center;
    float radius;
    std::uint8_t material;
};
}
