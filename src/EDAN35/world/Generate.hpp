#pragma once

#include "Chunk.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <glm/glm.hpp>

namespace world {
inline constexpr double SeaLevel = 0.0;
inline constexpr double TerrainFloor = -128.0;
inline constexpr double TerrainCeiling = 2048.0;
inline constexpr double TravelCeiling = 2048.0;

// Metres. Every field is hashed on integer lattices, so results are bit-identical across
// platforms; inputs are quantised to 1/8 m, which is the finest voxel-centre grid.
double terrainHeight(std::uint64_t seed, double x, double z);
// Rise over run from a central difference at 2 m spacing.
float terrainSlope(std::uint64_t seed, double x, double z);
std::uint8_t surfaceMaterial(std::uint64_t seed, double x, double z, double height, double depthBelow,
                             float slope);
bool caveAt(std::uint64_t seed, double x, double y, double z, float voxelSize);

// Any level: voxel solid iff its bottom is below terrainHeight at the column centre and not
// inside a cave wide enough for that level's voxel size. A voxel that is not solid is Water when
// its bottom is below SeaLevel (open sea, flooded caves), Air otherwise.
ChunkData generateChunk(std::uint64_t seed, ChunkKey key);
// One-time v4 snapshot conversion, in place: only Air that is procedural Water changes.
// Solid edits and Air carved into procedural solids are preserved.
void upgradeLegacyWater(std::uint64_t seed, ChunkKey key, ChunkData& data);
// Constant-time bounds only: Air above TerrainCeiling, Bedrock below TerrainFloor,
// nullopt otherwise; a value always equals isUniform of generateChunk(seed, key).
std::optional<std::uint8_t> trivialUniform(std::uint64_t seed, ChunkKey key);
bool isUniform(ChunkData const& data, std::uint8_t& value);
void overlaySaved(ChunkData& coarse, ChunkKey coarseKey, ChunkKey savedKey, ChunkData const& savedL0);

WorldPosition spawnPosition(std::uint64_t seed);
WorldPosition spawnTarget(std::uint64_t seed);

// 256-entry palette indexed by raw material byte (16 tints x 16 ids).
std::array<glm::vec3, 256> worldPalette();
}
