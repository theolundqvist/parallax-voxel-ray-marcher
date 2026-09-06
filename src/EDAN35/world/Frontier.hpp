#pragma once

#include "Chunk.hpp"
#include <map>
#include <vector>

namespace world {
struct Region {
    ChunkKey lo, hi;
    bool contains(ChunkKey k) const {
        return k.level == lo.level && k.x >= lo.x && k.x <= hi.x && k.y >= lo.y && k.y <= hi.y &&
               k.z >= lo.z && k.z <= hi.z;
    }
    template <class Visit> void each(Visit&& visit) const {
        for (auto z = lo.z; z <= hi.z; ++z)
        for (auto y = lo.y; y <= hi.y; ++y)
        for (auto x = lo.x; x <= hi.x; ++x) visit(ChunkKey{x, y, z, lo.level});
    }
};

// Even-aligned so every region is a union of whole parent chunks and nested inside the next level's region.
inline std::optional<Region> region(ChunkKey cameraL0, int level, int radius) {
    auto c = keyAtLevel(cameraL0, level);
    if (!c) return std::nullopt;
    auto lox = checkedAdd(c->x, -radius), loy = checkedAdd(c->y, -radius), loz = checkedAdd(c->z, -radius);
    auto hix = checkedAdd(c->x, radius), hiy = checkedAdd(c->y, radius), hiz = checkedAdd(c->z, radius);
    if (!lox || !loy || !loz || !hix || !hiy || !hiz) return std::nullopt;
    if (*hix == std::numeric_limits<std::int64_t>::max() || *hiy == std::numeric_limits<std::int64_t>::max() ||
        *hiz == std::numeric_limits<std::int64_t>::max()) return std::nullopt;
    auto lvl = std::uint8_t(level);
    return Region{{*lox & ~std::int64_t{1}, *loy & ~std::int64_t{1}, *loz & ~std::int64_t{1}, lvl},
                  {*hix | 1, *hiy | 1, *hiz | 1, lvl}};
}

inline bool covered(ChunkKey k, ChunkKey cameraL0, int radius) {
    if (k.level == 0) return false;
    auto finer = region(cameraL0, k.level - 1, radius);
    auto first = childOf(k, 0), last = childOf(k, 7);
    return finer && first && last && finer->contains(*first) && finer->contains(*last);
}

template <class Resident>
bool satisfied(ChunkKey k, ChunkKey cameraL0, int radius, Resident const& resident, std::map<ChunkKey, bool>& memo) {
    if (resident(k)) return true;
    if (!covered(k, cameraL0, radius)) return false;
    if (auto it = memo.find(k); it != memo.end()) return it->second;
    bool all = true;
    for (int child = 0; all && child < 8; ++child)
        all = satisfied(*childOf(k, child), cameraL0, radius, resident, memo);
    memo.emplace(k, all);
    return all;
}

template <class Resident>
void drawnBelow(ChunkKey k, ChunkKey cameraL0, int radius, Resident const& resident,
                std::map<ChunkKey, bool>& memo, std::vector<ChunkKey>& out) {
    bool isCovered = covered(k, cameraL0, radius);
    if (resident(k)) {
        bool refine = isCovered;
        for (int child = 0; refine && child < 8; ++child)
            refine = satisfied(*childOf(k, child), cameraL0, radius, resident, memo);
        if (!refine) {
            out.push_back(k);
            return;
        }
    } else if (!isCovered) {
        return;
    }
    for (int child = 0; child < 8; ++child) drawnBelow(*childOf(k, child), cameraL0, radius, resident, memo, out);
}

// Drawn keys never overlap and never leave their level's region; holes appear only where no ancestor is resident.
template <class Resident>
std::vector<ChunkKey> drawnSet(ChunkKey cameraL0, int radius, Resident const& resident) {
    std::vector<ChunkKey> out;
    auto top = region(cameraL0, LevelCount - 1, radius);
    if (!top) return out;
    std::map<ChunkKey, bool> memo;
    top->each([&](ChunkKey k) { drawnBelow(k, cameraL0, radius, resident, memo, out); });
    return out;
}

inline std::int64_t distanceSquared(ChunkKey a, ChunkKey b) {
    std::int64_t x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
    return x * x + y * y + z * z;
}

// Fine levels first: a coarse voxel around the eye reads as solid, so the camera's own detail must land before
// the horizon. Within a level, nearest first.
inline std::vector<ChunkKey> residencyTargets(ChunkKey cameraL0, int radius) {
    std::vector<ChunkKey> out;
    for (int level = 0; level < LevelCount; ++level) {
        auto r = region(cameraL0, level, radius);
        if (!r) continue;
        auto c = *keyAtLevel(cameraL0, level);
        auto begin = out.size();
        r->each([&](ChunkKey k) { out.push_back(k); });
        std::sort(out.begin() + begin, out.end(), [c](ChunkKey a, ChunkKey b) {
            auto da = distanceSquared(a, c), db = distanceSquared(b, c);
            return da == db ? a < b : da < db;
        });
    }
    return out;
}

inline bool retained(ChunkKey k, ChunkKey cameraL0, int radius) {
    auto r = region(cameraL0, k.level, radius + 1);
    return r && r->contains(k);
}
}
