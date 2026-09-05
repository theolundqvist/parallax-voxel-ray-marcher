#include "Generate.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

// Recipe (world units; voxel 0.25, chunk 8, y in [-32,48)):
//  * Islands live on three seeded jittered grids (Major 48, Minor 20,
//    Boulder 10). Major cell (0,0) is the forced spawn island (centre (0,4,0),
//    R 14, arch straddling x=8, cave entrance at azimuth 200 deg); its four
//    neighbour cells always exist with R >= 12.
//  * Island body: dens = smin(yTop-y, y-yBot, R-dxz) + detail, warped outline,
//    rolling meadow with rim dip, sqrt teardrop underside with jagged noise and
//    stalactite spikes. Boulders are noisy spheres. World dens = smooth max.
//  * Major islands may carry a stone torus arch; every Major island has a
//    forced entrance tunnel ending in a grotto plus noise tubes/chambers carved
//    only where the body is thick (dxz < 0.9R), so nothing is severed.
//  * Materials: crystal on cave wall shells, grass on open top faces, sand on
//    the rim, soil under the meadow, stone elsewhere with soil pockets.
//  * Costs: analytic geometry per voxel, noise on world-aligned lattices
//    (stride 1 for low frequencies, stride 0.5 for detail), chunk-level island
//    bbox rejection so most chunks return without touching a lattice.

namespace world {
namespace {

// ---------------------------------------------------------------- hashing --
inline std::uint32_t h32(std::uint64_t seed, std::int32_t ix, std::int32_t iy, std::int32_t iz,
                         std::uint32_t salt) {
    std::uint32_t x = std::uint32_t(ix) * 0x8DA6B343u ^ std::uint32_t(iy) * 0xD8163841u ^
                      std::uint32_t(iz) * 0xCB1AB31Fu ^ salt ^ std::uint32_t(seed) ^
                      std::uint32_t(seed >> 32) * 0x9E3779B9u;
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}
inline float unit(std::uint32_t h) { return float(h >> 8) * (1.0f / 16777216.0f); }
inline std::int32_t ifloor(float v) {
    int i = int(v);
    return v < float(i) ? i - 1 : i;
}
inline float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstep(float e0, float e1, float x) {
    float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
inline float smin(float a, float b, float k) {
    float h = std::max(k - std::fabs(a - b), 0.0f) / k;
    return std::min(a, b) - h * h * k * 0.25f;
}
inline float smax(float a, float b, float k) {
    float h = std::max(k - std::fabs(a - b), 0.0f) / k;
    return std::max(a, b) + h * h * k * 0.25f;
}

// ------------------------------------------------------------------ noise --
constexpr float Grad3[16][3] = {{1, 1, 0},  {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0}, {1, 0, 1},  {-1, 0, 1},
                                {1, 0, -1}, {-1, 0, -1}, {0, 1, 1}, {0, -1, 1},  {0, 1, -1}, {0, -1, -1},
                                {1, 1, 0},  {-1, 1, 0}, {0, -1, 1}, {0, -1, -1}};
constexpr float Grad2[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {0.7071068f, 0.7071068f},
                               {-0.7071068f, 0.7071068f}, {0.7071068f, -0.7071068f}, {-0.7071068f, -0.7071068f}};

float noise3(std::uint64_t seed, glm::vec3 p, std::uint32_t salt) {
    std::int32_t ix = ifloor(p.x), iy = ifloor(p.y), iz = ifloor(p.z);
    float fx = p.x - float(ix), fy = p.y - float(iy), fz = p.z - float(iz);
    auto g = [&](int dx, int dy, int dz) {
        auto const& v = Grad3[h32(seed, ix + dx, iy + dy, iz + dz, salt) & 15];
        return v[0] * (fx - float(dx)) + v[1] * (fy - float(dy)) + v[2] * (fz - float(dz));
    };
    float u = fade(fx), v = fade(fy), w = fade(fz);
    float x00 = lerp(g(0, 0, 0), g(1, 0, 0), u), x10 = lerp(g(0, 1, 0), g(1, 1, 0), u);
    float x01 = lerp(g(0, 0, 1), g(1, 0, 1), u), x11 = lerp(g(0, 1, 1), g(1, 1, 1), u);
    return lerp(lerp(x00, x10, v), lerp(x01, x11, v), w);
}
float noise2(std::uint64_t seed, glm::vec2 p, std::uint32_t salt) {
    std::int32_t ix = ifloor(p.x), iz = ifloor(p.y);
    float fx = p.x - float(ix), fz = p.y - float(iz);
    auto g = [&](int dx, int dz) {
        auto const& v = Grad2[h32(seed, ix + dx, 0x2D2D, iz + dz, salt) & 7];
        return v[0] * (fx - float(dx)) + v[1] * (fz - float(dz));
    };
    float u = fade(fx), w = fade(fz);
    return 1.4142136f * lerp(lerp(g(0, 0), g(1, 0), u), lerp(g(0, 1), g(1, 1), u), w);
}
float fbm3(std::uint64_t seed, glm::vec3 p, int octaves, std::uint32_t salt) {
    float sum = 0, amp = 1, norm = 0;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * noise3(seed, p, salt + std::uint32_t(i) * 0x9E3779B9u);
        norm += amp;
        amp *= 0.5f;
        p *= 2.0f;
    }
    return sum / norm;
}
float fbm2(std::uint64_t seed, glm::vec2 p, int octaves, std::uint32_t salt) {
    float sum = 0, amp = 1, norm = 0;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * noise2(seed, p, salt + std::uint32_t(i) * 0x9E3779B9u);
        norm += amp;
        amp *= 0.5f;
        p *= 2.0f;
    }
    return sum / norm;
}

// Salts: one per field so every field is independent.
enum Salt : std::uint32_t {
    SaltMajor = 0x4D414A01, SaltMinor = 0x4D494E02, SaltBoulder = 0x424F5503,
    SaltWarp = 0x57415250, SaltTop1 = 0x544F5031, SaltTop2 = 0x544F5032, SaltSoil = 0x534F494C,
    SaltBot1 = 0x424F5431, SaltBot2 = 0x424F5432, SaltDetail = 0x44455441, SaltPocket = 0x504F434B,
    SaltArch = 0x41524348, SaltBoulderN = 0x424F554E, SaltCaveA = 0x43415641, SaltCaveB = 0x43415642,
    SaltChamber = 0x4348414D, SaltCrystal = 0x43525953, SaltVoxel = 0x564F5845,
};

// --------------------------------------------------------------- lattices --
// Voxel v (0..32; 32 = first row of the chunk above, for the "open above"
// test) is sampled at its centre (v+0.5)*0.25 in chunk-local units.
struct Axis {
    int i0;
    float f;
};
// Voxels -1..32 are evaluated (one voxel of padding on every side) so the
// isolated-voxel filter sees true neighbours across chunk faces. Lattices
// therefore start one cell before the chunk origin.
constexpr int Pad = 1;
constexpr int Fine = 19;   // stride 0.5 -> covers [-0.5, 8.5]
constexpr int Coarse = 11; // stride 1   -> covers [-1, 9]
inline Axis fineAxis(int v) { return {((v + 2) >> 1), (v & 1) ? 0.75f : 0.25f}; }
inline Axis coarseAxis(int v) { return {((v + 4) >> 2), 0.125f + float(v & 3) * 0.25f}; }

template <int N> struct Lattice3 {
    float v[N * N * N];
    float& at(int x, int y, int z) { return v[x + N * (y + N * z)]; }
    float sample(Axis ax, Axis ay, Axis az) const {
        float const* b = v + ax.i0 + N * (ay.i0 + N * az.i0);
        float x00 = lerp(b[0], b[1], ax.f), x10 = lerp(b[N], b[N + 1], ax.f);
        float x01 = lerp(b[N * N], b[N * N + 1], ax.f), x11 = lerp(b[N * N + N], b[N * N + N + 1], ax.f);
        return lerp(lerp(x00, x10, ay.f), lerp(x01, x11, ay.f), az.f);
    }
};
template <int N> struct Lattice2 {
    float v[N * N];
    float& at(int x, int z) { return v[x + N * z]; }
    float sample(Axis ax, Axis az) const {
        float const* b = v + ax.i0 + N * az.i0;
        return lerp(lerp(b[0], b[1], ax.f), lerp(b[N], b[N + 1], ax.f), az.f);
    }
};

template <int N>
void fill3(Lattice3<N>& L, std::uint64_t seed, glm::vec3 origin, float stride, float scale, int octaves,
           std::uint32_t salt) {
    origin -= stride; // one padding cell before the chunk
    for (int z = 0; z < N; ++z)
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x)
                L.at(x, y, z) = fbm3(seed, (origin + glm::vec3(x, y, z) * stride) * scale, octaves, salt);
}
template <int N>
void fill2(Lattice2<N>& L, std::uint64_t seed, glm::vec2 origin, float stride, float scale, int octaves,
           std::uint32_t salt) {
    origin -= stride;
    for (int z = 0; z < N; ++z)
        for (int x = 0; x < N; ++x) L.at(x, z) = fbm2(seed, (origin + glm::vec2(x, z) * stride) * scale, octaves, salt);
}

// ---------------------------------------------------------------- islands --
enum class Kind : std::uint8_t { Major, Minor, Boulder };
struct Island {
    Kind kind;
    glm::vec3 c;
    float R, D;
    std::uint32_t id;
    glm::vec3 lo, hi; // conservative solid bounds (body + arch + noise)
    // Arch (Major only)
    bool arch = false;
    glm::vec3 archC;  // ring centre
    glm::vec2 archT;  // ring plane tangent (xz)
    glm::vec2 archN;  // ring plane normal (xz)
    float archA = 0, archTube = 0;
    glm::vec3 archLo, archHi;
    // Forced entrance tunnel (polyline along the body's mid-thickness) + grotto
    // at the last point (Major only).
    static constexpr int TunnelPoints = 6;
    glm::vec3 tunnel[TunnelPoints];
    float tunnelR = 0, grottoR = 0;
};
constexpr float ShellWidth = 0.7f;
// Slack added to every analytic bound: detail noise 0.4 + smooth-max bump 0.25.
constexpr float DensSlack = 1.0f;
// Camera 6 units above the meadow and 5.5 units outside the rim, pitched
// -12 deg and yawed ~10 deg right: the rim edge enters at the bottom of a
// 45 deg frame, the arch (ring centre (4.4, ~5, 7.6), crown ~y12) fills the
// right third with sky above, and the forced cave mouth (rim azimuth 90 deg,
// facing the camera, near (0, ~0.5, 14.7)) lies straight below the pose.
constexpr float SpawnPos[3] = {0.0f, 10.0f, 22.0f};
constexpr float SpawnLook[3] = {4.0f, 5.5f, 0.0f};

struct Layer {
    float S, P, Rmin, Rmax, yMin, yMax;
    std::uint32_t salt;
};
constexpr Layer MajorLayer{48, 0.75f, 10, 16, -4, 8, SaltMajor};
constexpr Layer MinorLayer{20, 0.5f, 3.5f, 7, -14, 18, SaltMinor};
constexpr Layer BoulderLayer{10, 0.25f, 1, 2.2f, -24, 30, SaltBoulder};

inline Layer const& layerOf(Kind k) {
    return k == Kind::Major ? MajorLayer : k == Kind::Minor ? MinorLayer : BoulderLayer;
}

// Warped horizontal distance and meadow height at an arbitrary world point
// (used for the arch ring centre; the chunk path uses lattices of the same
// fields so the two agree up to interpolation).
float warpAt(std::uint64_t seed, Island const& I, glm::vec2 xz) {
    return fbm2(seed, xz * (2.0f / I.R), 3, SaltWarp ^ I.id);
}
float yTopOf(Island const& I, float dxz, float top1, float top2) {
    return I.c.y + 1.2f * top1 + 0.5f * top2 - 2.0f * smoothstep(0.55f, 1.0f, dxz / I.R);
}

inline std::int32_t cellOf(float v, float S) { return ifloor(v / S + 0.5f); }

// Decides whether the island for grid cell (cx,cz) of a layer exists and
// fills it in. Forced spawn geometry is seed independent.
bool placeIsland(std::uint64_t seed, Kind kind, std::int32_t cx, std::int32_t cz, Island& I) {
    Layer const& L = layerOf(kind);
    auto draw = [&](int k) { return unit(h32(seed, cx, k, cz, L.salt)); };
    bool forcedSpawn = kind == Kind::Major && cx == 0 && cz == 0;
    bool forcedNeighbour = kind == Kind::Major && std::abs(cx) + std::abs(cz) == 1;
    if (!forcedSpawn && !forcedNeighbour && draw(0) >= L.P) return false;

    I.kind = kind;
    I.id = h32(seed, cx, 0x1D, cz, L.salt);
    // Cells are centred on multiples of S (cell (0,0) is centred on the
    // origin), so the spawn island sits mid-cell and its neighbours are a
    // full cell away in every direction.
    float jitter = L.S * 0.5f - L.Rmax;
    glm::vec2 centre = glm::vec2(cx, cz) * L.S + (glm::vec2(draw(1), draw(2)) * 2.0f - 1.0f) * jitter;
    I.R = L.Rmin + (L.Rmax - L.Rmin) * draw(3);
    float y = L.yMin + (L.yMax - L.yMin) * draw(4);
    if (forcedSpawn) {
        centre = glm::vec2(0);
        I.R = 14;
        y = 4;
    } else if (forcedNeighbour) {
        I.R = 12 + 4 * draw(3);
    }
    I.c = glm::vec3(centre.x, y, centre.y);
    I.D = 0.9f * I.R;

    if (kind == Kind::Boulder) {
        float e = I.R + 0.25f + DensSlack;
        I.lo = I.c - e;
        I.hi = I.c + e;
        return true;
    }
    // Body extent: outline warp 0.18R; top noise 1.7; underside noise 1.5 + spikes 3.
    float e = 1.18f * I.R + DensSlack;
    I.lo = glm::vec3(I.c.x - e, I.c.y - I.D - 1.5f - 3.0f - DensSlack, I.c.z - e);
    I.hi = glm::vec3(I.c.x + e, I.c.y + 1.2f + 0.5f + DensSlack, I.c.z + e);
    if (kind == Kind::Minor) return true;

    // Arch: ring of radius A in the vertical plane spanned by the rim tangent,
    // centred 0.75A inside the rim so both legs land inside the warped outline.
    I.arch = forcedSpawn || draw(5) < 0.6f;
    if (I.arch) {
        float A = std::clamp(0.5f * I.R, 4.0f, 8.0f);
        float inset = I.R - 0.75f * A;
        // Spawn arch: plane normal at 60 deg so the ring (centre x 4.4, half
        // span 6.1) straddles the x=8 chunk seam and faces the spawn camera
        // obliquely instead of edge-on.
        float az = forcedSpawn ? 1.0471976f : draw(6) * 6.2831853f;
        glm::vec2 dir(std::cos(az), std::sin(az));
        glm::vec2 r0 = glm::vec2(I.c.x, I.c.z) + inset * dir;
        float dxz = inset + 0.18f * I.R * warpAt(seed, I, r0);
        float top1 = fbm2(seed, r0 / 9.0f, 3, SaltTop1), top2 = fbm2(seed, r0 / 2.5f, 2, SaltTop2);
        I.archC = glm::vec3(r0.x, yTopOf(I, dxz, top1, top2), r0.y);
        I.archT = glm::vec2(-dir.y, dir.x);
        I.archN = dir;
        I.archA = A;
        I.archTube = 1.2f + 0.6f * draw(7);
        float reach = A + I.archTube + 0.3f + 0.25f; // ring + tube + tube noise + margin
        I.archLo = glm::vec3(r0.x - reach, I.archC.y - 1.5f - I.archTube - 0.55f, r0.y - reach);
        I.archHi = glm::vec3(r0.x + reach, I.archC.y + reach, r0.y + reach);
        I.lo = glm::min(I.lo, I.archLo);
        I.hi = glm::max(I.hi, I.archHi);
    }
    // Forced cave entrance: a tunnel from just outside the rim that follows
    // the noise-free mid-thickness of the body inward to a grotto at 0.35R.
    // The rim lip is thinner than the tube, so it starts as a notch/gorge and
    // becomes a roofed tunnel once the body is thick; being radial it can
    // never sever the island.
    float ez = forcedSpawn ? 90.0f * 0.017453292f : draw(8) * 6.2831853f;
    glm::vec2 edir(std::cos(ez), std::sin(ez));
    constexpr float Radii[Island::TunnelPoints] = {1.05f, 0.95f, 0.85f, 0.75f, 0.6f, 0.35f};
    for (int k = 0; k < Island::TunnelPoints; ++k) {
        float r = Radii[k];
        float top = I.c.y - 2.0f * smoothstep(0.55f, 1.0f, r);
        float bottom = I.c.y - I.D * std::sqrt(std::max(0.0f, 1.0f - r * r));
        I.tunnel[k] = glm::vec3(I.c.x + r * I.R * edir.x, 0.5f * (top + bottom), I.c.z + r * I.R * edir.y);
    }
    I.tunnelR = std::clamp(0.1f * I.R, 1.0f, 1.3f);
    I.grottoR = std::clamp(0.18f * I.R, 1.6f, 3.0f);
    return true;
}

// Minor/Boulder islands are suppressed near Major islands and the spawn pose.
bool nearMajor(std::uint64_t seed, glm::vec3 c, float margin) {
    std::int32_t cx = cellOf(c.x, MajorLayer.S), cz = cellOf(c.z, MajorLayer.S);
    for (std::int32_t z = cz - 1; z <= cz + 1; ++z)
        for (std::int32_t x = cx - 1; x <= cx + 1; ++x) {
            Island M;
            if (!placeIsland(seed, Kind::Major, x, z, M)) continue;
            glm::vec2 d(c.x - M.c.x, c.z - M.c.z);
            if (glm::dot(d, d) < (M.R + margin) * (M.R + margin)) return true;
        }
    return false;
}
bool nearSpawn(glm::vec3 c) {
    glm::vec3 d = c - glm::vec3(SpawnPos[0], SpawnPos[1], SpawnPos[2]);
    return glm::dot(d, d) < 12.0f * 12.0f;
}

// Every island whose bounds can touch the chunk. The cell range is derived
// from the chunk AABB grown by the layer's maximum reach, so no island that
// could contribute is ever skipped.
void collect(std::uint64_t seed, Kind kind, glm::vec3 lo, glm::vec3 hi, std::vector<Island>& out) {
    Layer const& L = layerOf(kind);
    float reach = 1.18f * L.Rmax + 8.0f; // covers body, arch and noise slack from any centre
    std::int32_t x0 = cellOf(lo.x - reach, L.S), x1 = cellOf(hi.x + reach, L.S);
    std::int32_t z0 = cellOf(lo.z - reach, L.S), z1 = cellOf(hi.z + reach, L.S);
    for (std::int32_t cz = z0; cz <= z1; ++cz)
        for (std::int32_t cx = x0; cx <= x1; ++cx) {
            Island I;
            if (!placeIsland(seed, kind, cx, cz, I)) continue;
            if (glm::any(glm::lessThan(I.hi, lo)) || glm::any(glm::greaterThan(I.lo, hi))) continue;
            if (kind != Kind::Major && nearSpawn(I.c)) continue;
            if (kind == Kind::Minor && nearMajor(seed, I.c, 1.18f * I.R + 3.0f)) continue;
            out.push_back(I);
        }
}

// ---------------------------------------------------------------- scratch --
// Per (island, column): the body is a height-field slab [yBot, yTop] cut at
// the warped outline, so every column is a single vertical interval and
// neighbouring intervals always overlap (connectivity by construction).
struct Column {
    float dxz, yTop, yBot, detailAmp;
    bool live;
};
enum Flag : std::uint8_t { Solid = 1, Body = 2, Arch = 4, Shell = 8, Grotto = 16, Boulder = 32 };
struct Voxel {
    std::uint8_t flags;
    std::int8_t owner;
};
constexpr int Span = ChunkSize + 2 * Pad; // evaluated voxels per axis (-1..32)

struct Scratch {
    Lattice3<Coarse> caveA, caveB, chamber, crystal;
    Lattice3<Fine> detail, pocket, archNoise, boulderNoise;
    Lattice2<Fine> top1, top2, bot1, bot2, soil;
    std::vector<Lattice2<Fine>> warp;
    std::vector<Column> columns; // islands * Span * Span
    Voxel voxels[Span * Span * Span];
    Voxel& at(int x, int y, int z) { return voxels[(x + Pad) + Span * ((y + Pad) + Span * (z + Pad))]; }
    static size_t column(int island, int x, int z) { return size_t(island) * Span * Span + (x + Pad) + Span * (z + Pad); }
};

inline float segmentDistance(glm::vec3 p, glm::vec3 a, glm::vec3 b) {
    glm::vec3 ab = b - a;
    float t = std::clamp(glm::dot(p - a, ab) / glm::dot(ab, ab), 0.0f, 1.0f);
    return glm::length(p - (a + ab * t));
}

std::uint8_t tinted(Material m, std::uint32_t tint) { return std::uint8_t(m | (tint << 4)); }

} // namespace

ChunkData generateChunk(std::uint64_t seed, ChunkKey key) {
    ChunkData data{};
    glm::vec3 origin = world::origin(key);
    glm::vec3 chunkLo = origin, chunkHi = origin + ChunkSpan;
    // Voxels -1..32 are evaluated (padding for the isolated-voxel filter and
    // the open-above test), so the island query box is grown by one voxel.
    glm::vec3 probeLo = chunkLo - VoxelScale, probeHi = chunkHi + VoxelScale;

    std::vector<Island> islands;
    collect(seed, Kind::Major, probeLo, probeHi, islands);
    collect(seed, Kind::Minor, probeLo, probeHi, islands);
    collect(seed, Kind::Boulder, probeLo, probeHi, islands);
    if (islands.empty()) return data;

    bool anyMajor = false, anyArch = false, anyBoulder = false, anyBody = false;
    for (auto const& I : islands) {
        anyMajor |= I.kind == Kind::Major;
        anyArch |= I.arch;
        anyBoulder |= I.kind == Kind::Boulder;
        anyBody |= I.kind != Kind::Boulder;
    }

    auto S = std::make_unique<Scratch>();
    glm::vec2 originXZ(origin.x, origin.z);
    if (anyBody) {
        fill2(S->bot1, seed, originXZ, 0.5f, 1.0f / 4.0f, 2, SaltBot1);
        fill2(S->bot2, seed, originXZ, 0.5f, 1.0f / 3.0f, 2, SaltBot2);
        fill3(S->detail, seed, origin, 0.5f, 1.0f / 1.5f, 2, SaltDetail);
        fill2(S->top1, seed, originXZ, 0.5f, 1.0f / 9.0f, 3, SaltTop1);
        fill2(S->top2, seed, originXZ, 0.5f, 1.0f / 2.5f, 2, SaltTop2);
        fill2(S->soil, seed, originXZ, 0.5f, 1.0f / 3.0f, 1, SaltSoil);
    }
    fill3(S->pocket, seed, origin, 0.5f, 1.0f / 2.0f, 2, SaltPocket);
    if (anyMajor) {
        fill3(S->caveA, seed, origin, 1.0f, 1.0f / 6.0f, 3, SaltCaveA);
        fill3(S->caveB, seed, origin, 1.0f, 1.0f / 6.0f, 3, SaltCaveB);
        fill3(S->chamber, seed, origin, 1.0f, 1.0f / 5.0f, 2, SaltChamber);
        fill3(S->crystal, seed, origin, 1.0f, 1.0f / 7.0f, 1, SaltCrystal);
    }
    if (anyArch) fill3(S->archNoise, seed, origin, 0.5f, 1.0f / 2.0f, 2, SaltArch);
    if (anyBoulder) fill3(S->boulderNoise, seed, origin, 0.5f, 2.0f, 1, SaltBoulderN);

    // Per-island outline warp and per-column geometry.
    int n = int(islands.size());
    S->warp.resize(n);
    S->columns.resize(size_t(n) * Span * Span);
    for (int i = 0; i < n; ++i) {
        Island const& I = islands[i];
        if (I.kind != Kind::Boulder) fill2(S->warp[i], seed, originXZ, 0.5f, 2.0f / I.R, 3, SaltWarp ^ I.id);
        for (int vz = -Pad; vz < ChunkSize + Pad; ++vz)
            for (int vx = -Pad; vx < ChunkSize + Pad; ++vx) {
                Column& col = S->columns[Scratch::column(i, vx, vz)];
                glm::vec2 xz = originXZ + (glm::vec2(vx, vz) + 0.5f) * VoxelScale;
                glm::vec2 d = xz - glm::vec2(I.c.x, I.c.z);
                float dist = glm::length(d);
                if (I.kind == Kind::Boulder) {
                    col = {dist, 0, 0, 0, dist < I.R + 0.25f + DensSlack};
                    continue;
                }
                Axis ax = fineAxis(vx), az = fineAxis(vz);
                float dxz = dist + 0.18f * I.R * S->warp[i].sample(ax, az);
                col.dxz = dxz;
                col.live = I.R - dxz > -DensSlack;
                if (!col.live) continue;
                col.yTop = yTopOf(I, dxz, S->top1.sample(ax, az), S->top2.sample(ax, az));
                float r = dxz / I.R;
                // Teardrop underside, jagged (bot1) with hanging stalactite
                // curtains (ridge of bot2); all terms extend downward only and
                // the slab keeps a minimum thickness so the rim never thins to
                // a wedge that voxelizes into crumbs.
                float ridge = 1.0f - std::fabs(S->bot2.sample(ax, az));
                float spike = 3.0f * std::clamp((ridge - 0.6f) * 5.0f, 0.0f, 1.0f);
                float yBot = I.c.y - I.D * std::sqrt(std::max(0.0f, 1.0f - r * r)) - 1.5f * S->bot1.sample(ax, az) - spike;
                col.yBot = std::min(yBot, col.yTop - 1.0f);
                col.detailAmp = 0.4f * smoothstep(1.0f, 2.5f, col.yTop - col.yBot);
            }
    }

    // Pass 1: occupancy, ownership and cave flags for voxels -1..32.
    for (int vz = -Pad; vz < ChunkSize + Pad; ++vz) {
        Axis fz = fineAxis(vz), cz = coarseAxis(vz);
        for (int vx = -Pad; vx < ChunkSize + Pad; ++vx) {
            Axis fx = fineAxis(vx), cx = coarseAxis(vx);
            float px = origin.x + (vx + 0.5f) * VoxelScale, pz = origin.z + (vz + 0.5f) * VoxelScale;
            for (int vy = -Pad; vy < ChunkSize + Pad; ++vy) {
                Voxel& V = S->at(vx, vy, vz);
                V = {0, -1};
                Axis fy = fineAxis(vy), cy = coarseAxis(vy);
                float py = origin.y + (vy + 0.5f) * VoxelScale;
                glm::vec3 p(px, py, pz);

                float best = 0;
                int owner = -1;
                bool haveDetail = false;
                float detail = 0;
                for (int i = 0; i < n; ++i) {
                    Column const& col = S->columns[Scratch::column(i, vx, vz)];
                    if (!col.live) continue;
                    Island const& I = islands[i];
                    float d;
                    if (I.kind == Kind::Boulder) {
                        if (py < I.lo.y || py > I.hi.y) continue;
                        d = I.R - glm::length(p - I.c) - 0.25f * S->boulderNoise.sample(fx, fy, fz);
                    } else {
                        if (py > col.yTop + DensSlack || py < col.yBot - DensSlack) continue;
                        if (!haveDetail) {
                            detail = S->detail.sample(fx, fy, fz);
                            haveDetail = true;
                        }
                        d = std::min(smin(col.yTop - py, py - col.yBot, 1.5f), I.R - col.dxz) + col.detailAmp * detail;
                    }
                    if (owner < 0) {
                        best = d;
                        owner = i;
                    } else {
                        if (d > best) owner = i;
                        best = smax(best, d, 1.0f);
                    }
                }

                // Stone arches are combined after the body and never carved.
                bool archSolid = false;
                for (int i = 0; i < n; ++i) {
                    Island const& I = islands[i];
                    if (!I.arch || glm::any(glm::lessThan(p, I.archLo)) || glm::any(glm::greaterThan(p, I.archHi)))
                        continue;
                    glm::vec3 q = p - I.archC;
                    if (q.y < -1.5f) continue;
                    float qu = q.x * I.archT.x + q.z * I.archT.y, qn = q.x * I.archN.x + q.z * I.archN.y;
                    float ring = std::sqrt(qu * qu + q.y * q.y) - I.archA;
                    float ad = I.archTube + 0.3f * S->archNoise.sample(fx, fy, fz) - std::sqrt(ring * ring + qn * qn);
                    if (ad > 0) {
                        archSolid = true;
                        if (owner < 0 || ad > best) {
                            best = ad;
                            owner = i;
                        }
                    }
                }
                if (owner < 0 || !(best > 0)) continue;

                Island const& I = islands[owner];
                std::uint8_t flags = Solid;
                if (archSolid) {
                    flags |= Arch;
                } else if (I.kind == Kind::Boulder) {
                    flags |= Boulder;
                } else {
                    flags |= Body;
                    if (I.kind == Kind::Major) {
                        Column const& col = S->columns[Scratch::column(owner, vx, vz)];
                        // Forced entrance tunnel + grotto (wall noise from the pocket field).
                        float grotto = glm::length(p - I.tunnel[Island::TunnelPoints - 1]) - I.grottoR;
                        float tunnel = grotto;
                        for (int k = 0; k + 1 < Island::TunnelPoints; ++k)
                            tunnel = std::min(tunnel, segmentDistance(p, I.tunnel[k], I.tunnel[k + 1]) - I.tunnelR);
                        float wall = 0.3f * S->pocket.sample(fx, fy, fz);
                        if (tunnel - wall < 0) {
                            flags &= ~Solid;
                        } else if (grotto - wall < ShellWidth) {
                            flags |= Shell | Grotto; // crystals only deep in the grotto, not at the mouth
                        }
                        // Noise tubes/chambers only where the body is thick and above
                        // the underside, so they breach the meadow (entrances) but
                        // never the belly.
                        if (col.dxz < 0.9f * I.R && py > col.yBot + 1.5f) {
                            float a = S->caveA.sample(cx, cy, cz), b = S->caveB.sample(cx, cy, cz);
                            float ab = a * a + b * b;
                            float ch = S->chamber.sample(cx, cy, cz);
                            bool deep = best > 2.5f;
                            if (ab < 0.012f || (deep && ch > 0.62f)) flags &= ~Solid;
                            else if (ab < 0.035f || (deep && ch > 0.55f)) flags |= Shell;
                        }
                    }
                }
                V.flags = flags;
                V.owner = std::int8_t(owner);
            }
        }
    }

    // Pass 2: materials for solid voxels of rows 0..31.
    for (int vz = 0; vz < ChunkSize; ++vz) {
        Axis fz = fineAxis(vz), cz = coarseAxis(vz);
        for (int vx = 0; vx < ChunkSize; ++vx) {
            Axis fx = fineAxis(vx), cx = coarseAxis(vx);
            for (int vy = 0; vy < ChunkSize; ++vy) {
                Voxel const& V = S->at(vx, vy, vz);
                if (!(V.flags & Solid)) continue;
                // Isolated voxels (no solid face neighbour) are the only
                // voxelization crumbs the continuous fields can leave; drop
                // them. Neighbours come from the padded evaluation, so both
                // chunks sharing a face make the same decision.
                if (!((S->at(vx - 1, vy, vz).flags | S->at(vx + 1, vy, vz).flags | S->at(vx, vy - 1, vz).flags |
                       S->at(vx, vy + 1, vz).flags | S->at(vx, vy, vz - 1).flags | S->at(vx, vy, vz + 1).flags) & Solid))
                    continue;
                Axis fy = fineAxis(vy), cy = coarseAxis(vy);
                Island const& I = islands[V.owner];
                std::int32_t gx = key.x * ChunkSize + vx, gy = key.y * ChunkSize + vy, gz = key.z * ChunkSize + vz;
                std::uint32_t h = h32(seed, gx, gy, gz, SaltVoxel);
                std::uint32_t tint = h & 3;
                float py = origin.y + (vy + 0.5f) * VoxelScale;
                std::uint8_t& out = data[index(vx, vy, vz)];

                bool openAbove = !(S->at(vx, vy + 1, vz).flags & (Body | Arch | Boulder));
                if (V.flags & Boulder) {
                    out = S->pocket.sample(fx, fy, fz) > 0.5f ? tinted(Soil, tint) : tinted(Stone, tint);
                    continue;
                }
                Column const& col = S->columns[Scratch::column(V.owner, vx, vz)];
                // Arch voxels may sit over a dead column; they only grow grass on open tops.
                float dTop = (V.flags & Arch) ? -100.0f : col.yTop - py;
                if (V.flags & Shell) {
                    float c = S->crystal.sample(cx, cy, cz);
                    if (c > ((V.flags & Grotto) ? 0.0f : 0.45f) && (h & 0xFF) < 90) {
                        std::uint32_t hue = c > 0.6f ? 0 : S->chamber.sample(cx, cy, cz) > 0 ? 1 : 2;
                        out = tinted(Crystal, hue);
                        continue;
                    }
                }
                bool archTop = !(V.flags & Arch) || py > I.archC.y + 0.7f * I.archA;
                if (dTop < 0.35f && openAbove && archTop) {
                    out = tinted(Grass, tint);
                } else if (!(V.flags & Arch) && col.dxz > 0.8f * I.R && dTop < 0.75f) {
                    out = tinted(Sand, tint);
                } else if (!(V.flags & Arch) && dTop < 1.5f + 0.7f * S->soil.sample(fx, fz)) {
                    out = tinted(Soil, tint);
                } else if (S->pocket.sample(fx, fy, fz) > 0.5f) {
                    out = tinted(Soil, tint);
                } else {
                    bool underside = !(V.flags & Arch) && dTop > 3.0f;
                    out = tinted(Stone, underside ? 8 + tint : tint);
                }
            }
        }
    }
    return data;
}

glm::vec3 spawnPosition(std::uint64_t) { return glm::vec3(SpawnPos[0], SpawnPos[1], SpawnPos[2]); }
glm::vec3 spawnTarget(std::uint64_t) { return glm::vec3(SpawnLook[0], SpawnLook[1], SpawnLook[2]); }

std::array<glm::vec3, 256> worldPalette() {
    auto rgb = [](std::uint32_t hex) {
        return glm::vec3((hex >> 16) & 255, (hex >> 8) & 255, hex & 255) / 255.0f;
    };
    glm::vec3 const grass = rgb(0x5FA53E), soil = rgb(0x7A5230), stoneWarm = rgb(0x8A8C8E),
                    stoneCool = rgb(0x7E858C), sand = rgb(0xD9C68A);
    glm::vec3 const crystal[3] = {rgb(0x7FE6FF), rgb(0xC77DFF), rgb(0xFFB3E6)};
    std::array<glm::vec3, 256> palette{};
    for (int tint = 0; tint < 16; ++tint) {
        float value = 1.0f + 0.06f * (float(tint & 3) - 1.5f) / 1.5f;
        for (int id = 0; id < 16; ++id) {
            glm::vec3 c(0.5f);
            switch (id) {
            case Air: c = glm::vec3(0); break;
            case Grass: c = grass * value; break;
            case Soil: c = soil * value; break;
            case Stone: c = (tint >= 8 ? stoneCool : stoneWarm) * value; break;
            case Sand: c = sand * value; break;
            case Crystal: c = crystal[tint % 3]; break;
            default: break;
            }
            palette[size_t(id | (tint << 4))] = c;
        }
    }
    return palette;
}
}
