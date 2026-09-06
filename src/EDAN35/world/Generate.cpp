#include "Generate.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <mutex>

namespace world {
namespace {

// World coordinates are int64 eighth-metres: exact for every representable chunk, and the
// voxel centres of every level lie on that grid.
constexpr std::int64_t Q = 8;
constexpr std::int64_t ChunkUnits = std::int64_t(ChunkSize) * 2;

inline std::int64_t toUnits(double metres) {
    return std::llround(std::clamp(metres * double(Q), -9.2e18, 9.2e18));
}
inline double toMetres(std::int64_t units) { return double(units) / double(Q); }
inline std::int64_t wrapAdd(std::int64_t a, std::int64_t b) {
    return std::int64_t(std::uint64_t(a) + std::uint64_t(b));
}

// ---------------------------------------------------------------- hashing --
inline std::uint64_t mix(std::uint64_t x) {
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBull;
    x ^= x >> 31;
    return x;
}
inline std::uint64_t hash(std::uint64_t seed, std::uint64_t a, std::uint64_t b, std::uint64_t c,
                          std::uint32_t salt) {
    std::uint64_t x = mix(seed ^ (a * 0x9E3779B97F4A7C15ull) ^ salt);
    x = mix(x ^ (b * 0xC2B2AE3D27D4EB4Full));
    return mix(x ^ (c * 0x165667B19E3779F9ull));
}
inline double signedUnit(std::uint64_t h) {
    return double(h >> 11) * (1.0 / 4503599627370496.0) - 1.0;
}
inline double lerp(double a, double b, double t) { return a + (b - a) * t; }
inline double smooth(double t) { return t * t * (3.0 - 2.0 * t); }
inline double smoothstep(double e0, double e1, double x) {
    return smooth(std::clamp((x - e0) / (e1 - e0), 0.0, 1.0));
}

struct Cell {
    std::int64_t index;
    double t;
};
inline Cell cellOf(std::int64_t v, std::int64_t period) {
    std::int64_t q = v / period, r = v % period;
    if (r < 0) {
        q -= 1;
        r += period;
    }
    return {q, double(r) / double(period)};
}
inline std::int64_t floorDiv(std::int64_t v, std::int64_t d) { return cellOf(v, d).index; }

enum Salt : std::uint32_t {
    SaltContinent = 0x434F4E54,
    SaltWarpX = 0x57415250,
    SaltWarpZ = 0x57415251,
    SaltRidge = 0x52494447,
    SaltDetail = 0x44455441,
    SaltMicro = 0x4D494352,
    SaltTint = 0x54494E54,
    SaltCaveA = 0x43415641,
    SaltCaveB = 0x43415642,
    SaltCaveR = 0x43415652,
};

// ------------------------------------------------------------------ noise --
// Pythagorean rotations keep the lattice integer-exact while breaking axis alignment
// between octaves; the period is scaled by the hypotenuse to compensate.
struct Rotation {
    std::int64_t a, b, scale;
};
constexpr Rotation Rotations[4] = {{1, 0, 1}, {4, 3, 5}, {12, 5, 13}, {15, 8, 17}};

inline double value2(std::uint64_t seed, std::int64_t x, std::int64_t z, std::int64_t period,
                     std::uint32_t salt, int octave = 0) {
    Rotation const& rot = Rotations[octave & 3];
    std::uint64_t ux = std::uint64_t(x), uz = std::uint64_t(z), a = std::uint64_t(rot.a), b = std::uint64_t(rot.b);
    std::int64_t px = std::int64_t(a * ux + b * uz), pz = std::int64_t(a * uz - b * ux);
    Cell cx = cellOf(px, period * rot.scale), cz = cellOf(pz, period * rot.scale);
    std::uint64_t ix = std::uint64_t(cx.index), iz = std::uint64_t(cz.index);
    double v00 = signedUnit(hash(seed, ix, iz, 0, salt));
    double v10 = signedUnit(hash(seed, ix + 1, iz, 0, salt));
    double v01 = signedUnit(hash(seed, ix, iz + 1, 0, salt));
    double v11 = signedUnit(hash(seed, ix + 1, iz + 1, 0, salt));
    double tx = smooth(cx.t), tz = smooth(cz.t);
    return lerp(lerp(v00, v10, tx), lerp(v01, v11, tx), tz);
}

inline double fbm2(std::uint64_t seed, std::int64_t x, std::int64_t z, std::int64_t period,
                   int octaves, std::uint32_t salt) {
    double sum = 0, amp = 1, norm = 0;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * value2(seed, x, z, period, salt + std::uint32_t(i), i);
        norm += amp;
        amp *= 0.5;
        period /= 2;
    }
    return sum / norm;
}

// One 3D lattice field with a one-cell memo: a chunk spans far less than a cave cell, so
// nearly every voxel reuses the eight hashed corners.
struct Lattice3 {
    std::uint64_t seed;
    std::int64_t period;
    std::uint32_t salt;
    std::int64_t cx = 0, cy = 0, cz = 0;
    bool valid = false;
    double corner[8] = {};

    double at(std::int64_t x, std::int64_t y, std::int64_t z) {
        Cell ax = cellOf(x, period), ay = cellOf(y, period), az = cellOf(z, period);
        if (!valid || ax.index != cx || ay.index != cy || az.index != cz) {
            cx = ax.index;
            cy = ay.index;
            cz = az.index;
            valid = true;
            for (int i = 0; i < 8; ++i)
                corner[i] = signedUnit(hash(seed, std::uint64_t(cx) + (i & 1),
                                            std::uint64_t(cy) + ((i >> 1) & 1),
                                            std::uint64_t(cz) + (i >> 2), salt));
        }
        double tx = smooth(ax.t), ty = smooth(ay.t), tz = smooth(az.t);
        double x00 = lerp(corner[0], corner[1], tx), x10 = lerp(corner[2], corner[3], tx);
        double x01 = lerp(corner[4], corner[5], tx), x11 = lerp(corner[6], corner[7], tx);
        return lerp(lerp(x00, x10, ty), lerp(x01, x11, ty), tz);
    }
};

// ---------------------------------------------------------------- terrain --
constexpr double SnowLine = 1100.0;
constexpr double RockSlope = 0.83909963117728;
constexpr double SandSlope = 0.21255656167002;
constexpr double CaveRadiusMin = 1.0, CaveRadiusMax = 2.9;
constexpr double CaveFloor = TerrainFloor + 8.0;

// Range skeleton: ridges are the zero contours of warped value noise (connected curves),
// valleys its extremes. Every octave is additive on its own amplitude so side ridges and
// valleys exist everywhere on land, not only on the flanks of main crests.
// Bound: 180 + 1000 + 400 + 220 + 140 + 30 + 1 = 1971 < TerrainCeiling.
double heightAt(std::uint64_t seed, std::int64_t x, std::int64_t z) {
    double continent = fbm2(seed, x, z, 6000 * Q, 3, SaltContinent);
    double base = 30.0 + 150.0 * continent;
    double warpX = 900.0 * fbm2(seed, x, z, 3000 * Q, 4, SaltWarpX);
    double warpZ = 900.0 * fbm2(seed, x, z, 3000 * Q, 4, SaltWarpZ);
    std::int64_t rx = wrapAdd(x, toUnits(warpX)), rz = wrapAdd(z, toUnits(warpZ));
    double crest[4];
    std::int64_t period = 5000 * Q;
    for (int i = 0; i < 4; ++i) {
        double n = std::fabs(value2(seed, rx, rz, period, SaltRidge + std::uint32_t(i), i + 1));
        crest[i] = 1.0 - std::min(n * 1.2, 1.0);
        period /= 2;
    }
    double ridge = 1000.0 * crest[0] * std::sqrt(crest[0]) + 400.0 * crest[1] + 220.0 * crest[2] + 140.0 * crest[3];
    double mask = smoothstep(-0.2, 0.05, continent);
    double detail = 30.0 * fbm2(seed, x, z, 150 * Q, 2, SaltDetail);
    double micro = value2(seed, x, z, 8 * Q, SaltMicro);
    double h = base + mask * ridge + detail + micro;
    assert(h < TerrainCeiling);
    return std::max(h, TerrainFloor);
}

float slopeAt(std::uint64_t seed, std::int64_t x, std::int64_t z) {
    constexpr std::int64_t step = 2 * Q;
    double dx = (heightAt(seed, wrapAdd(x, step), z) - heightAt(seed, wrapAdd(x, -step), z)) / 4.0;
    double dz = (heightAt(seed, x, wrapAdd(z, step)) - heightAt(seed, x, wrapAdd(z, -step))) / 4.0;
    return float(std::sqrt(dx * dx + dz * dz));
}

inline std::uint8_t tintAt(std::uint64_t seed, std::int64_t x, std::int64_t z) {
    return std::uint8_t(hash(seed, std::uint64_t(floorDiv(x, 2 * Q)), std::uint64_t(floorDiv(z, 2 * Q)),
                             0, SaltTint) & 15);
}

inline std::uint8_t materialAt(double height, double top, float slope, std::uint8_t tint) {
    if (top <= TerrainFloor) return Bedrock;
    double depth = height - top;
    std::uint8_t id;
    if (depth >= 2.0) id = Stone;
    else if (depth >= 0.5) id = Soil;
    else if (height > SnowLine) id = Snow;
    else if (slope > RockSlope) id = Rock;
    else if (height < SeaLevel + 2.0) id = Sand;
    else id = Grass;
    return std::uint8_t(id | (tint << 4));
}

struct Caves {
    Lattice3 a, b, radius;
    explicit Caves(std::uint64_t seed)
        : a{seed, 96 * Q, SaltCaveA}, b{seed, 96 * Q, SaltCaveB}, radius{seed, 400 * Q, SaltCaveR} {}

    bool open(std::int64_t x, std::int64_t y, std::int64_t z, double voxelSize) {
        if (toMetres(y) <= CaveFloor) return false;
        double r = CaveRadiusMin + (CaveRadiusMax - CaveRadiusMin) * (0.5 + 0.5 * radius.at(x, y, z));
        if (r < 1.5 * voxelSize) return false;
        double av = a.at(x, y, z), bv = b.at(x, y, z);
        double limit = r * 0.04;
        return av * av + bv * bv < limit * limit;
    }
};
constexpr double CaveLevelLimit = CaveRadiusMax / 1.5;

// ------------------------------------------------------------------ spawn --
struct Spawn {
    glm::dvec3 eye, target;
};

// Eye on a low shoulder (100..320 m) looking up at least 1000 m to a summit >= 1500 m at
// 2..3.5 km over a clear sight line, with water or a second summit in the look cone for
// depth; among valid summits the steepest look angle wins.
Spawn searchSpawn(std::uint64_t seed) {
    constexpr double eyeHeight = 1.8;
    constexpr double SummitMin = 1500.0, RiseMin = 1000.0, SecondSummit = 1400.0;
    constexpr double MinSlope = 0.32491969623290634;
    auto ground = [&](double x, double z) { return heightAt(seed, toUnits(x), toUnits(z)); };
    auto clear = [&](glm::dvec3 eye, glm::dvec3 target) {
        constexpr double step = 25.0, clearance = 5.0;
        double dist = std::hypot(target.x - eye.x, target.z - eye.z);
        for (double s = step; s < dist; s += step) {
            double t = s / dist;
            double line = eye.y + (target.y - eye.y) * t;
            if (ground(eye.x + (target.x - eye.x) * t, eye.z + (target.z - eye.z) * t) > line - clearance)
                return false;
        }
        return true;
    };
    // Water within 3 km or another summit >= 1400 m within 4 km on one of five bearings
    // spanning -30..30 degrees around the look direction.
    auto depth = [&](glm::dvec3 eye, glm::dvec3 target) {
        constexpr double step = 100.0, waterReach = 3000.0, summitReach = 4000.0, apart = 300.0;
        constexpr double cosine[5] = {0.8660254037844387, 0.9659258262890683, 1.0, 0.9659258262890683,
                                      0.8660254037844387};
        constexpr double sine[5] = {-0.5, -0.25881904510252074, 0.0, 0.25881904510252074, 0.5};
        double dist = std::hypot(target.x - eye.x, target.z - eye.z);
        double dx = (target.x - eye.x) / dist, dz = (target.z - eye.z) / dist;
        for (int bearing = 0; bearing < 5; ++bearing) {
            double bx = dx * cosine[bearing] - dz * sine[bearing], bz = dx * sine[bearing] + dz * cosine[bearing];
            for (double s = step; s <= summitReach; s += step) {
                double x = eye.x + bx * s, z = eye.z + bz * s, h = ground(x, z);
                if (s <= waterReach && h < SeaLevel) return true;
                if (h >= SecondSummit && std::hypot(x - target.x, z - target.z) >= apart) return true;
            }
        }
        return false;
    };
    Caves caves(seed);
    // Summits are rare, eyes are common: walk summits outward from the origin and, for each,
    // pick the eye in the 2..3.5 km annulus with the steepest valid look angle.
    auto eyeFor = [&](glm::dvec3 peak, Spawn& out, double& bestSlope) {
        constexpr std::int64_t reach = 3500 * Q, near = 2000 * Q, step = 100 * Q;
        std::int64_t x = toUnits(peak.x), z = toUnits(peak.z);
        bool found = false;
        for (std::int64_t dz = -reach; dz <= reach; dz += step)
            for (std::int64_t dx = -reach; dx <= reach; dx += step) {
                std::int64_t d2 = dx * dx + dz * dz;
                if (d2 > reach * reach || d2 < near * near) continue;
                std::int64_t px = wrapAdd(x, dx), pz = wrapAdd(z, dz);
                double h = heightAt(seed, px, pz);
                if (h < 100.0 || h > 320.0) continue;
                double eyeY = h + eyeHeight;
                if (peak.y - eyeY < RiseMin) continue;
                double slope = (peak.y - eyeY) * double(Q) / std::sqrt(double(d2));
                if (slope <= bestSlope) continue;
                if (caves.open(px, toUnits(eyeY), pz, VoxelScale)) continue;
                glm::dvec3 eye(toMetres(px), eyeY, toMetres(pz));
                if (!clear(eye, peak) || !depth(eye, peak)) continue;
                bestSlope = slope;
                out = {eye, peak};
                found = true;
            }
        return found;
    };
    auto summit = [&](std::int64_t x, std::int64_t z, Spawn& out, double& bestSlope) {
        double h = heightAt(seed, x, z);
        if (h < SummitMin) return false;
        return eyeFor(glm::dvec3(toMetres(x), h, toMetres(z)), out, bestSlope);
    };
    constexpr std::int64_t step = 50 * Q;
    Spawn spawn{};
    for (std::int64_t ring = 0;; ring += step) {
        double bestSlope = MinSlope;
        for (std::int64_t v = -ring; v <= ring; v += step) {
            summit(ring, v, spawn, bestSlope);
            summit(-ring, v, spawn, bestSlope);
            summit(v, ring, spawn, bestSlope);
            summit(v, -ring, spawn, bestSlope);
        }
        if (bestSlope > MinSlope) return spawn;
    }
}

Spawn findSpawn(std::uint64_t seed) {
    static std::mutex mutex;
    static std::optional<std::pair<std::uint64_t, Spawn>> cached;
    std::lock_guard lock(mutex);
    if (!cached || cached->first != seed) cached = {seed, searchSpawn(seed)};
    return cached->second;
}

WorldPosition worldPosition(glm::dvec3 metres) {
    return *normalizedPosition(ChunkKey{0, 0, 0, 0}, metres);
}

} // namespace

double terrainHeight(std::uint64_t seed, double x, double z) {
    return heightAt(seed, toUnits(x), toUnits(z));
}

float terrainSlope(std::uint64_t seed, double x, double z) {
    return slopeAt(seed, toUnits(x), toUnits(z));
}

std::uint8_t surfaceMaterial(std::uint64_t seed, double x, double z, double height, double depthBelow,
                             float slope) {
    return materialAt(height, height - depthBelow, slope, tintAt(seed, toUnits(x), toUnits(z)));
}

bool caveAt(std::uint64_t seed, double x, double y, double z, float voxelSize) {
    Caves caves(seed);
    return caves.open(toUnits(x), toUnits(y), toUnits(z), voxelSize);
}

std::optional<std::uint8_t> trivialUniform(std::uint64_t, ChunkKey key) {
    double span = double(ChunkSpan) * double(std::int64_t{1} << key.level);
    double bottom = double(key.y) * span;
    if (bottom >= TerrainCeiling) return Air;
    if (bottom + span <= TerrainFloor) return Bedrock;
    return std::nullopt;
}

ChunkData generateChunk(std::uint64_t seed, ChunkKey key) {
    ChunkData data{};
    std::int64_t const voxel = 2 << key.level, span = ChunkUnits << key.level;
    auto corner = levelZeroCorner(key);
    auto x0 = corner ? checkedShiftLeft(corner->x, 6) : std::nullopt;
    auto y0 = corner ? checkedShiftLeft(corner->y, 6) : std::nullopt;
    auto z0 = corner ? checkedShiftLeft(corner->z, 6) : std::nullopt;
    if (!x0 || !y0 || !z0 || !checkedAdd(*x0, span) || !checkedAdd(*y0, span) || !checkedAdd(*z0, span)) {
        data.fill(key.y < 0 ? Bedrock : Air);
        return data;
    }
    double const voxelMetres = toMetres(voxel);
    double const chunkBottom = toMetres(*y0), chunkTop = toMetres(*y0 + span);
    bool const carve = voxelMetres < CaveLevelLimit && chunkTop > CaveFloor;
    Caves caves(seed);
    for (int vz = 0; vz < ChunkSize; ++vz) {
        std::int64_t z = *z0 + vz * voxel + voxel / 2;
        for (int vx = 0; vx < ChunkSize; ++vx) {
            std::int64_t x = *x0 + vx * voxel + voxel / 2;
            double h = heightAt(seed, x, z);
            if (chunkBottom >= h) continue;
            float slope = 0;
            if (chunkTop > h - 0.5 && h <= SnowLine && h > TerrainFloor) slope = slopeAt(seed, x, z);
            std::uint8_t tint = tintAt(seed, x, z);
            for (int vy = 0; vy < ChunkSize; ++vy) {
                std::int64_t bottom = *y0 + vy * voxel;
                double bottomMetres = toMetres(bottom);
                if (bottomMetres >= h) break;
                if (carve && caves.open(x, bottom + voxel / 2, z, voxelMetres)) continue;
                data[size_t(index(vx, vy, vz))] = materialAt(h, bottomMetres + voxelMetres, slope, tint);
            }
        }
    }
    return data;
}

bool isUniform(ChunkData const& data, std::uint8_t& value) {
    value = data[0];
    return std::all_of(data.begin(), data.end(), [&](std::uint8_t v) { return v == value; });
}

void overlaySaved(ChunkData& coarse, ChunkKey coarseKey, ChunkKey savedKey, ChunkData const& savedL0) {
    assert(coarseKey.level >= 1 && coarseKey.level <= OverlayLevels && savedKey.level == 0);
    int const level = coarseKey.level, block = 1 << level;
    auto corner = levelZeroCorner(coarseKey);
    assert(corner);
    std::int64_t ox = (savedKey.x - corner->x) * ChunkSize, oy = (savedKey.y - corner->y) * ChunkSize,
                 oz = (savedKey.z - corner->z) * ChunkSize;
    assert(ox >= 0 && oy >= 0 && oz >= 0 && ox < ChunkSize * block && oy < ChunkSize * block &&
           oz < ChunkSize * block);
    int const coarseX = int(ox >> level), coarseY = int(oy >> level), coarseZ = int(oz >> level);
    int const count = ChunkSize >> level;
    for (int cz = 0; cz < count; ++cz)
        for (int cy = 0; cy < count; ++cy)
            for (int cx = 0; cx < count; ++cx) {
                std::uint8_t material = Air;
                for (int fy = (cy + 1) * block - 1; fy >= cy * block && material == Air; --fy)
                    for (int fz = cz * block; fz < (cz + 1) * block && material == Air; ++fz)
                        for (int fx = cx * block; fx < (cx + 1) * block; ++fx) {
                            std::uint8_t v = savedL0[size_t(index(fx, fy, fz))];
                            if (v != Air) {
                                material = v;
                                break;
                            }
                        }
                coarse[size_t(index(coarseX + cx, coarseY + cy, coarseZ + cz))] = material;
            }
}

WorldPosition spawnPosition(std::uint64_t seed) { return worldPosition(findSpawn(seed).eye); }
WorldPosition spawnTarget(std::uint64_t seed) { return worldPosition(findSpawn(seed).target); }

std::array<glm::vec3, 256> worldPalette() {
    auto rgb = [](std::uint32_t hex) {
        return glm::vec3((hex >> 16) & 255, (hex >> 8) & 255, hex & 255) / 255.0f;
    };
    glm::vec3 const grass = rgb(0x5B9A3A), soil = rgb(0x7A5230), stone = rgb(0x8A8C8E), sand = rgb(0xD9C68A),
                    snow = rgb(0xF2F6FA), rock = rgb(0x6B6864), bedrock = rgb(0x2E2E30);
    glm::vec3 const crystal[3] = {rgb(0x7FE6FF), rgb(0xC77DFF), rgb(0xFFB3E6)};
    std::array<glm::vec3, 256> palette{};
    for (int tint = 0; tint < 16; ++tint) {
        float value = 1.0f + 0.06f * (float(tint & 3) - 1.5f) / 1.5f;
        float hue = 0.04f * (float(tint >> 2) - 1.5f) / 1.5f;
        for (int id = 0; id < 16; ++id) {
            glm::vec3 c(0.5f);
            switch (id) {
            case Air: c = glm::vec3(0); break;
            case Grass: c = (grass + glm::vec3(hue, 0, -hue)) * value; break;
            case Soil: c = soil * value; break;
            case Stone: c = stone * value; break;
            case Sand: c = sand * value; break;
            case Crystal: c = crystal[tint % 3]; break;
            case Snow: c = snow * (1.0f + 0.02f * (value - 1.0f)); break;
            case Rock: c = rock * value; break;
            case Bedrock: c = bedrock; break;
            default: break;
            }
            palette[size_t(id | (tint << 4))] = c;
        }
    }
    return palette;
}
}
