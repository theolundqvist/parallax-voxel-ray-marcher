#include "../world/Generate.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace world;
using Bytes = std::vector<std::uint8_t>;
constexpr std::uint64_t Seed = DefaultSeed;
constexpr std::uint64_t ExpectedFingerprint = 0x9f54eeef78c40793ull;

void require(bool value, std::string const& message) {
    if (!value) throw std::runtime_error(message);
}

struct Column {
    double x, z, height;
};
Column columnAt(ChunkKey key, int vx, int vz) {
    auto corner = *levelZeroCorner(key);
    double voxel = voxelSizeAt(key.level);
    double x = double(corner.x) * ChunkSpan + (vx + 0.5) * voxel;
    double z = double(corner.z) * ChunkSpan + (vz + 0.5) * voxel;
    return {x, z, terrainHeight(Seed, x, z)};
}
double chunkBottom(ChunkKey key) { return double((*levelZeroCorner(key)).y) * ChunkSpan; }

// Highest solid voxel top in metres, or -infinity for an empty column.
double columnTop(ChunkData const& data, ChunkKey key, int vx, int vz) {
    double voxel = voxelSizeAt(key.level);
    for (int vy = ChunkSize - 1; vy >= 0; --vy)
        if (data[size_t(index(vx, vy, vz))] != Air) return chunkBottom(key) + (vy + 1) * voxel;
    return -INFINITY;
}

ChunkKey keyAround(glm::dvec3 metres, int level) {
    auto position = *normalizedPosition(ChunkKey{0, 0, 0, 0}, metres);
    return *keyAtLevel(position.anchor, level);
}

// Every voxel follows the published rule: this is what cross-chunk continuity and level
// consistency reduce to, checked exhaustively instead of by neighbour comparison alone.
void checkRule(ChunkKey key) {
    auto data = generateChunk(Seed, key);
    double voxel = voxelSizeAt(key.level), bottom = chunkBottom(key);
    for (int vz = 0; vz < ChunkSize; ++vz)
        for (int vx = 0; vx < ChunkSize; ++vx) {
            auto column = columnAt(key, vx, vz);
            for (int vy = 0; vy < ChunkSize; ++vy) {
                double voxelBottom = bottom + vy * voxel;
                bool solid = voxelBottom < column.height &&
                             !caveAt(Seed, column.x, voxelBottom + voxel / 2, column.z, float(voxel));
                auto material = data[size_t(index(vx, vy, vz))];
                require((material != Air) == solid,
                        "Solidity rule violated at level " + std::to_string(key.level) + " key " +
                            std::to_string(key.x) + "," + std::to_string(key.y) + "," + std::to_string(key.z) +
                            " voxel " + std::to_string(vx) + "," + std::to_string(vy) + "," + std::to_string(vz) +
                            " h " + std::to_string(column.height) + " bottom " + std::to_string(voxelBottom) +
                            " cave " + std::to_string(caveAt(Seed, column.x, voxelBottom + voxel / 2, column.z, float(voxel))));
                if (solid && voxelBottom + voxel <= TerrainFloor)
                    require(material == Bedrock, "Bedrock expected below TerrainFloor");
            }
        }
}

void checkLevelConsistency(ChunkKey key) {
    auto data = generateChunk(Seed, key);
    double voxel = voxelSizeAt(key.level), bottom = chunkBottom(key), top = bottom + ChunkSpan * (1 << key.level);
    for (int vz = 0; vz < ChunkSize; ++vz)
        for (int vx = 0; vx < ChunkSize; ++vx) {
            auto column = columnAt(key, vx, vz);
            if (column.height <= bottom || column.height >= top) continue;
            double expectedTop = bottom + voxel * std::floor((column.height - bottom) / voxel + 1.0);
            if (caveAt(Seed, column.x, expectedTop - voxel / 2, column.z, float(voxel))) continue;
            double actual = columnTop(data, key, vx, vz);
            require(actual > column.height && actual <= column.height + voxel,
                    "Column top not within one voxel of terrainHeight at level " + std::to_string(key.level));
        }
}

void checkSeam(ChunkKey a, int axis) {
    auto b = *offsetKey(a, axis == 0 ? glm::ivec3(1, 0, 0) : glm::ivec3(0, 0, 1));
    auto dataA = generateChunk(Seed, a), dataB = generateChunk(Seed, b);
    double voxel = voxelSizeAt(a.level);
    for (int i = 0; i < ChunkSize; ++i) {
        int ax = axis == 0 ? ChunkSize - 1 : i, az = axis == 0 ? i : ChunkSize - 1;
        int bx = axis == 0 ? 0 : i, bz = axis == 0 ? i : 0;
        auto ca = columnAt(a, ax, az), cb = columnAt(b, bx, bz);
        double ta = columnTop(dataA, a, ax, az), tb = columnTop(dataB, b, bx, bz);
        if (!std::isfinite(ta) || !std::isfinite(tb)) continue;
        // A carved surface voxel lowers the column top; compare only uncarved surfaces.
        double low = std::min(ta, tb) - voxel / 2, high = std::max(ca.height, cb.height) + voxel;
        bool cave = false;
        for (double y = low; y <= high && !cave; y += voxel)
            cave = caveAt(Seed, ca.x, y, ca.z, float(voxel)) || caveAt(Seed, cb.x, y, cb.z, float(voxel));
        if (cave) continue;
        require(std::fabs(ta - tb) <= std::fabs(ca.height - cb.height) + 2 * voxel,
                "Seam discontinuity at level " + std::to_string(a.level));
    }
}

void checkOverlay(int level, std::mt19937_64& rng) {
    auto spawn = spawnPosition(Seed);
    glm::dvec3 eye = glm::dvec3(spawn.anchor.x, spawn.anchor.y, spawn.anchor.z) * double(ChunkSpan) + spawn.offset;
    ChunkKey coarseKey = keyAround(eye, level);
    auto coarse = generateChunk(Seed, coarseKey);
    auto reference = coarse;
    int block = 1 << level;
    for (int child = 0; child < block * block * block; ++child) {
        auto corner = *levelZeroCorner(coarseKey);
        ChunkKey saved{corner.x + child % block, corner.y + (child / block) % block, corner.z + child / (block * block), 0};
        auto fine = generateChunk(Seed, saved);
        for (int i = 0; i < 64; ++i) fine[rng() % fine.size()] = std::uint8_t(rng() % 2 ? Crystal : Air);
        overlaySaved(coarse, coarseKey, saved, fine);
        int ox = (child % block) * (ChunkSize / block), oy = ((child / block) % block) * (ChunkSize / block),
            oz = (child / (block * block)) * (ChunkSize / block);
        for (int cz = 0; cz < ChunkSize / block; ++cz)
            for (int cy = 0; cy < ChunkSize / block; ++cy)
                for (int cx = 0; cx < ChunkSize / block; ++cx) {
                    std::uint8_t expected = Air;
                    for (int fy = (cy + 1) * block - 1; fy >= cy * block && expected == Air; --fy)
                        for (int fz = cz * block; fz < (cz + 1) * block && expected == Air; ++fz)
                            for (int fx = cx * block; fx < (cx + 1) * block; ++fx)
                                if (auto v = fine[size_t(index(fx, fy, fz))]; v != Air) {
                                    expected = v;
                                    break;
                                }
                    require(coarse[size_t(index(ox + cx, oy + cy, oz + cz))] == expected,
                            "Overlay is not the topmost-solid downsample at level " + std::to_string(level));
                }
    }
    bool differs = coarse != reference;
    require(differs, "Overlay of edited children left the coarse chunk untouched");
}

void checkSpawn() {
    auto a = spawnPosition(Seed), b = spawnPosition(Seed);
    require(a.anchor == b.anchor && a.offset == b.offset, "spawnPosition is not deterministic");
    auto ta = spawnTarget(Seed), tb = spawnTarget(Seed);
    require(ta.anchor == tb.anchor && ta.offset == tb.offset, "spawnTarget is not deterministic");
    auto metres = [](WorldPosition p) {
        return glm::dvec3(p.anchor.x, p.anchor.y, p.anchor.z) * double(ChunkSpan) + p.offset;
    };
    auto eye = metres(a), target = metres(ta);
    require(eye.y >= 100.0 && eye.y <= 322.0, "Spawn height outside [100, 320]");
    require(terrainHeight(Seed, eye.x, eye.z) < eye.y, "Spawn eye is inside terrain");
    glm::dvec3 look = target - eye;
    double dist = std::hypot(look.x, look.z);
    require(dist >= 2000.0 && dist <= 3500.0, "Summit target outside 2..3.5 km");
    require(target.y >= 1500.0 && std::fabs(terrainHeight(Seed, target.x, target.z) - target.y) < 1e-9,
            "Spawn target is not a summit of at least 1500 m");
    require(look.y >= 1000.0, "Summit rises less than 1000 m above the eye");
    require(look.y / dist > 0.32491969623290634, "Look angle below 18 degrees");
    for (double s = 25.0; s < dist; s += 25.0) {
        double t = s / dist;
        double line = eye.y + look.y * t;
        double ground = terrainHeight(Seed, eye.x + look.x * t, eye.z + look.z * t);
        require(ground <= line - 5.0, "Terrain blocks the spawn sight line at " + std::to_string(s) + " m");
    }
    bool depth = false;
    for (int bearing = -2; bearing <= 2 && !depth; ++bearing) {
        double angle = bearing * 15.0 * 0.017453292519943295;
        double bx = (look.x * std::cos(angle) - look.z * std::sin(angle)) / dist;
        double bz = (look.x * std::sin(angle) + look.z * std::cos(angle)) / dist;
        for (double s = 100.0; s <= 4000.0 && !depth; s += 100.0) {
            double x = eye.x + bx * s, z = eye.z + bz * s, h = terrainHeight(Seed, x, z);
            depth = (s <= 3000.0 && h < SeaLevel) ||
                    (h >= 1400.0 && std::hypot(x - target.x, z - target.z) >= 300.0);
        }
    }
    require(depth, "Neither water within 3 km nor a second summit within 4 km in the look cone");
    std::cout << "spawn eye (" << eye.x << ", " << eye.y << ", " << eye.z << ") target (" << target.x << ", "
              << target.y << ", " << target.z << ") distance " << dist << " m, look angle "
              << std::atan2(look.y, dist) * 57.29577951308232 << " deg\n";
}

// Order-sensitive FNV-1a over 1000 keys across levels within 4 km of the origin; the value
// is the generator's identity for GeneratorVersion and must not depend on the spawn.
std::uint64_t fingerprint() {
    std::mt19937_64 rng(11);
    std::uniform_real_distribution<double> around(-4000, 4000), height(TerrainFloor - 16, 1800);
    std::uint64_t value = 1469598103934665603ull;
    for (int i = 0; i < 1000; ++i) {
        int level = i % LevelCount;
        ChunkKey key = keyAround(glm::dvec3(around(rng), height(rng), around(rng)), level);
        for (auto byte : generateChunk(Seed, key)) {
            value ^= byte;
            value *= 1099511628211ull;
        }
    }
    return value;
}

void checkCaves() {
    std::mt19937_64 rng(7);
    std::uniform_real_distribution<double> xz(-4000, 4000), y(TerrainFloor, 1200);
    int fine = 0;
    for (int i = 0; i < 200000; ++i) {
        double px = xz(rng), py = y(rng), pz = xz(rng);
        require(!caveAt(Seed, px, py, pz, voxelSizeAt(3)), "Cave open at level 3");
        require(!caveAt(Seed, px, py, pz, voxelSizeAt(5)), "Cave open at level 5");
        fine += caveAt(Seed, px, py, pz, voxelSizeAt(0));
    }
    require(fine > 0, "No caves at level 0");
    std::cout << "level-0 cave fraction " << double(fine) / 200000 << "\n";
}

void checkTrivial() {
    int nontrivial = 0, decided = 0;
    for (int level = 0; level < LevelCount; ++level)
        for (std::int64_t y = -6; y <= 6; ++y) {
            ChunkKey key{3 >> level, y, -5 >> level, std::uint8_t(level)};
            auto trivial = trivialUniform(Seed, key);
            std::uint8_t value;
            bool uniform = isUniform(generateChunk(Seed, key), value);
            if (trivial) {
                require(uniform && value == *trivial, "trivialUniform disagrees with generateChunk");
                ++decided;
            } else if (!uniform) ++nontrivial;
        }
    require(decided > 0 && nontrivial > 0, "trivialUniform coverage fixture is degenerate");
    ChunkKey far{std::int64_t{1} << 62, std::int64_t{1} << 62, -(std::int64_t{1} << 62), 0};
    std::uint8_t value;
    require(isUniform(generateChunk(Seed, far), value) && value == *trivialUniform(Seed, far),
            "Unrepresentable chunk disagrees with trivialUniform");
}

// ---------------------------------------------------------------- png --
std::uint32_t crc32(std::span<const std::uint8_t> bytes, std::uint32_t crc = 0) {
    static const auto table = [] {
        std::array<std::uint32_t, 256> result{};
        for (std::uint32_t i = 0; i != 256; ++i) {
            auto value = i;
            for (int bit = 0; bit != 8; ++bit) value = (value & 1) ? (value >> 1) ^ 0xedb88320u : value >> 1;
            result[i] = value;
        }
        return result;
    }();
    crc ^= 0xffffffffu;
    for (auto byte : bytes) crc = table[(crc ^ byte) & 255] ^ (crc >> 8);
    return crc ^ 0xffffffffu;
}
void putBE(Bytes& out, std::uint32_t v) {
    for (int i = 3; i >= 0; --i) out.push_back(std::uint8_t(v >> (i * 8)));
}
void chunk(Bytes& out, char const* type, Bytes const& body) {
    putBE(out, std::uint32_t(body.size()));
    Bytes typed(type, type + 4);
    typed.insert(typed.end(), body.begin(), body.end());
    out.insert(out.end(), typed.begin(), typed.end());
    putBE(out, crc32(typed));
}
void writePng(std::string const& path, int width, int height, Bytes const& rgb) {
    Bytes raw;
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), rgb.begin() + size_t(y) * width * 3, rgb.begin() + size_t(y + 1) * width * 3);
    }
    Bytes z{0x78, 0x01};
    std::uint32_t a = 1, b = 0;
    for (auto byte : raw) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    for (size_t at = 0; at < raw.size(); at += 65535) {
        size_t n = std::min<size_t>(65535, raw.size() - at);
        z.push_back(at + n == raw.size());
        z.push_back(std::uint8_t(n)), z.push_back(std::uint8_t(n >> 8));
        z.push_back(std::uint8_t(~n)), z.push_back(std::uint8_t(~n >> 8));
        z.insert(z.end(), raw.begin() + at, raw.begin() + at + n);
    }
    putBE(z, (b << 16) | a);
    Bytes png{0x89, 'P', 'N', 'G', 13, 10, 26, 10}, ihdr;
    putBE(ihdr, std::uint32_t(width)), putBE(ihdr, std::uint32_t(height));
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
    chunk(png, "IHDR", ihdr);
    chunk(png, "IDAT", z);
    chunk(png, "IEND", {});
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<char const*>(png.data()), std::streamsize(png.size()));
}

void writeHeightmap(std::string const& path) {
    auto palette = worldPalette();
    auto spawn = spawnPosition(Seed);
    glm::dvec3 eye = glm::dvec3(spawn.anchor.x, spawn.anchor.y, spawn.anchor.z) * double(ChunkSpan) + spawn.offset;
    constexpr int size = 1024;
    constexpr double metresPerPixel = 8000.0 / size;
    Bytes rgb(size_t(size) * size * 3);
    for (int py = 0; py < size; ++py)
        for (int px = 0; px < size; ++px) {
            double x = eye.x + (px - size / 2) * metresPerPixel, z = eye.z + (py - size / 2) * metresPerPixel;
            double h = terrainHeight(Seed, x, z);
            float slope = terrainSlope(Seed, x, z);
            double dx = (terrainHeight(Seed, x + 8, z) - terrainHeight(Seed, x - 8, z)) / 16.0;
            double dz = (terrainHeight(Seed, x, z + 8) - terrainHeight(Seed, x, z - 8)) / 16.0;
            double lit = (0.5 * dx + 0.5 * dz + 0.6) / (std::sqrt(dx * dx + dz * dz + 1.0) * std::sqrt(0.86));
            double shade = 0.2 + 1.0 * std::clamp(lit, 0.0, 1.0);
            glm::vec3 colour;
            if (h < SeaLevel) {
                double depth = std::clamp(-h / 60.0, 0.0, 1.0);
                colour = glm::mix(glm::vec3(0.25f, 0.55f, 0.75f), glm::vec3(0.02f, 0.12f, 0.35f), float(depth));
            } else {
                float elevation = float(std::clamp(h / 1600.0, 0.0, 1.0));
                colour = glm::mix(palette[surfaceMaterial(Seed, x, z, h, 0.0, slope)], glm::vec3(0.95f), 0.35f * elevation) * float(shade);
            }
            bool marker = std::hypot(px - size / 2, py - size / 2) < 3.0;
            if (marker) colour = glm::vec3(1, 0, 0);
            for (int c = 0; c < 3; ++c)
                rgb[(size_t(py) * size + px) * 3 + c] = std::uint8_t(std::clamp(colour[c], 0.0f, 1.0f) * 255.0f);
        }
    writePng(path, size, size, rgb);
}
} // namespace

int main(int argc, char** argv) {
    try {
        auto start = std::chrono::steady_clock::now();
        auto spawn = spawnPosition(Seed);
        glm::dvec3 eye = glm::dvec3(spawn.anchor.x, spawn.anchor.y, spawn.anchor.z) * double(ChunkSpan) + spawn.offset;
        std::cout << "spawn search " << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() << " s\n";
        checkSpawn();
        std::mt19937_64 rng(3);
        std::uniform_real_distribution<double> around(-1500, 1500);
        for (int level = 0; level < LevelCount; ++level) {
            for (int i = 0; i < 3; ++i) {
                glm::dvec3 p = eye + glm::dvec3(around(rng), 0, around(rng));
                p.y = terrainHeight(Seed, p.x, p.z);
                ChunkKey key = keyAround(p, level);
                checkRule(key);
                checkLevelConsistency(key);
                checkSeam(key, 0);
                checkSeam(key, 1);
            }
            // 2^46 chunks: 5.6e14 m, where doubles still resolve the 1/8 m voxel-centre grid exactly.
            ChunkKey far = keyAround(glm::dvec3(double(std::int64_t{1} << 46) * ChunkSpan, 10, -3e14), level);
            checkRule(far);
            checkLevelConsistency(far);
            checkSeam(far, 0);
            checkSeam(far, 1);
        }
        for (int level = 1; level <= OverlayLevels; ++level) checkOverlay(level, rng);
        checkCaves();
        checkTrivial();
        std::uint64_t print = fingerprint();
        std::cout << "fingerprint " << std::hex << print << std::dec << "\n";
        require(print == ExpectedFingerprint, "Generator fingerprint changed: bump GeneratorVersion and pin the new value");
        auto t0 = std::chrono::steady_clock::now();
        int chunks = 0;
        for (int level = 0; level < LevelCount; ++level)
            for (int i = 0; i < 4; ++i, ++chunks) {
                ChunkKey key = keyAround(eye + glm::dvec3(i * 8, -4 * (1 << level), 0), level);
                generateChunk(Seed, key);
            }
        std::cout << "generateChunk mean " << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / chunks << " ms over levels 0..9\n";
        if (argc > 1) {
            writeHeightmap(argv[1]);
            std::cout << "heightmap " << argv[1] << "\n";
        }
        std::cout << "world_generate_test OK\n";
        return 0;
    } catch (std::exception const& error) {
        std::cerr << "FAIL: " << error.what() << "\n";
        return 1;
    }
}
