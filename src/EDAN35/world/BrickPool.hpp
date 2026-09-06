#pragma once

#include "Chunk.hpp"
#include <glad/glad.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace world {
inline constexpr glm::ivec3 PoolBricks{16, 20, 16};
static_assert(PoolBricks.x * PoolBricks.y * PoolBricks.z == BrickCapacity);

// R8 pool of ChunkSize^3 bricks; R8 8^3-cell presence masks: opaque=1, water=2, air=4.
class BrickPool {
public:
    static constexpr int capacity = BrickCapacity;
    BrickPool() = default;
    BrickPool(BrickPool const&) = delete;
    BrickPool& operator=(BrickPool const&) = delete;
    ~BrickPool();

    void init();
    std::optional<std::uint16_t> allocate();
    void release(std::uint16_t slot);
    std::size_t upload(std::uint16_t slot, ChunkData const& data);
    int used() const { return capacity - int(free.size()); }
    GLuint texture() const { return pool; }
    GLuint occupancyTexture() const { return occupancy; }

private:
    GLuint pool = 0, occupancy = 0;
    std::vector<std::uint16_t> free;
};

// One R16UI PageSize x PageSize x (PageSize * LevelCount) atlas: level L owns slab z in
// [L*PageSize, (L+1)*PageSize) as a toroidal table. Entry 0 not drawn, 1..255 uniform material,
// 256+slot brick.
class LevelAtlas {
public:
    LevelAtlas() = default;
    LevelAtlas(LevelAtlas const&) = delete;
    LevelAtlas& operator=(LevelAtlas const&) = delete;
    ~LevelAtlas();

    void init();
    GLuint texture() const { return atlas; }

private:
    GLuint atlas = 0;
};

class LevelTable {
public:
    void init(LevelAtlas const& atlas, int level);
    void set(ChunkKey key, std::uint16_t entry);
    static glm::ivec3 texel(ChunkKey key);

private:
    GLuint atlas = 0;
    int slab = 0;
};
}
