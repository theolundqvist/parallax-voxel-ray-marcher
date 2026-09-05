#pragma once

#include "Chunk.hpp"
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

namespace world {
// Pure floating-island generator. Every voxel is a function of (seed, world
// position) only: island placement is a seeded jittered lattice, every noise
// field is evaluated on world-aligned lattices, so chunk faces are continuous
// by construction. Material byte: low 4 bits Material id, upper 4 bits tint
// (palette index = raw byte); air is always byte 0.
ChunkData generateChunk(std::uint64_t seed, ChunkKey key);

// Camera pose in front of the forced spawn island (never inside solid).
glm::vec3 spawnPosition(std::uint64_t seed);
glm::vec3 spawnTarget(std::uint64_t seed);

// 256-entry palette indexed by raw material byte (16 tints x 16 ids).
std::array<glm::vec3, 256> worldPalette();
}
