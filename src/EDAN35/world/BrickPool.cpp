#include "BrickPool.hpp"
#include <stdexcept>

namespace world {
namespace {
constexpr int OccupancyCells = ChunkSize / 8;

struct UnpackState {
    static constexpr GLenum settings[] = {GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH,
        GL_UNPACK_IMAGE_HEIGHT, GL_UNPACK_SKIP_PIXELS, GL_UNPACK_SKIP_ROWS, GL_UNPACK_SKIP_IMAGES};
    GLint previous[6], buffer;
    UnpackState() {
        for (int i = 0; i < 6; ++i) {
            glGetIntegerv(settings[i], &previous[i]);
            glPixelStorei(settings[i], i == 0 ? 1 : 0);
        }
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &buffer);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    ~UnpackState() {
        for (int i = 0; i < 6; ++i) glPixelStorei(settings[i], previous[i]);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, GLuint(buffer));
    }
};

GLuint texture3D(GLenum internalFormat, glm::ivec3 size, GLenum format, GLenum type) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_3D, id);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage3D(GL_TEXTURE_3D, 0, GLint(internalFormat), size.x, size.y, size.z, 0, format, type, nullptr);
    return id;
}

glm::ivec3 brickOf(std::uint16_t slot) {
    return {slot % PoolBricks.x, (slot / PoolBricks.x) % PoolBricks.y, slot / (PoolBricks.x * PoolBricks.y)};
}
}

BrickPool::~BrickPool() {
    glDeleteTextures(1, &pool);
    glDeleteTextures(1, &occupancy);
}

void BrickPool::init() {
    pool = texture3D(GL_R8, PoolBricks * ChunkSize, GL_RED, GL_UNSIGNED_BYTE);
    occupancy = texture3D(GL_R8, PoolBricks * OccupancyCells, GL_RED, GL_UNSIGNED_BYTE);
    free.resize(capacity);
    for (int i = 0; i < capacity; ++i) free[i] = std::uint16_t(capacity - 1 - i);
}

std::optional<std::uint16_t> BrickPool::allocate() {
    if (free.empty()) return std::nullopt;
    auto slot = free.back();
    free.pop_back();
    return slot;
}

void BrickPool::release(std::uint16_t slot) {
    if (slot >= capacity) throw std::invalid_argument("BrickPool slot out of range");
    free.push_back(slot);
}

std::size_t BrickPool::upload(std::uint16_t slot, ChunkData const& data) {
    if (slot >= capacity) throw std::invalid_argument("BrickPool slot out of range");
    // Presence bits distinguish homogeneous transparent cells from mixed water/air.
    std::uint8_t cells[OccupancyCells * OccupancyCells * OccupancyCells] = {};
    for (int z = 0; z < ChunkSize; ++z)
        for (int y = 0; y < ChunkSize; ++y)
            for (int x = 0; x < ChunkSize; ++x) {
                std::uint8_t v = data[index(x, y, z)];
                cells[(x / 8) + OccupancyCells * ((y / 8) + OccupancyCells * (z / 8))] |=
                    v == Air ? 4 : opaque(v) ? 1 : 2;
            }
    auto brick = brickOf(slot);
    UnpackState unpack;
    auto voxel = brick * ChunkSize;
    glBindTexture(GL_TEXTURE_3D, pool);
    glTexSubImage3D(GL_TEXTURE_3D, 0, voxel.x, voxel.y, voxel.z, ChunkSize, ChunkSize, ChunkSize,
                    GL_RED, GL_UNSIGNED_BYTE, data.data());
    auto cell = brick * OccupancyCells;
    glBindTexture(GL_TEXTURE_3D, occupancy);
    glTexSubImage3D(GL_TEXTURE_3D, 0, cell.x, cell.y, cell.z, OccupancyCells, OccupancyCells, OccupancyCells,
                    GL_RED, GL_UNSIGNED_BYTE, cells);
    return data.size() + sizeof(cells);
}

LevelAtlas::~LevelAtlas() { glDeleteTextures(1, &atlas); }

void LevelAtlas::init() {
    atlas = texture3D(GL_R16UI, glm::ivec3(PageSize, PageSize, PageSize * LevelCount), GL_RED_INTEGER,
                      GL_UNSIGNED_SHORT);
}

void LevelTable::init(LevelAtlas const& levels, int level) {
    atlas = levels.texture();
    slab = level * PageSize;
}

glm::ivec3 LevelTable::texel(ChunkKey key) {
    auto wrap = [](std::int64_t v) { return int(((v % PageSize) + PageSize) % PageSize); };
    return {wrap(key.x), wrap(key.y), wrap(key.z)};
}

void LevelTable::set(ChunkKey key, std::uint16_t entry) {
    auto at = texel(key);
    UnpackState unpack;
    glBindTexture(GL_TEXTURE_3D, atlas);
    glTexSubImage3D(GL_TEXTURE_3D, 0, at.x, at.y, slab + at.z, 1, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_SHORT, &entry);
}
}
