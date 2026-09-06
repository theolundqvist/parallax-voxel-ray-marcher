#include "../world/Frontier.hpp"
#include <algorithm>
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>

using namespace world;

namespace {
void require(bool value, char const* message) {
    if (!value) throw std::runtime_error(message);
}

struct Footprint {
    ChunkKey corner;
    std::int64_t span;
};
Footprint footprintOf(ChunkKey key) {
    return {*levelZeroCorner(key), std::int64_t{1} << key.level};
}
bool contains(Footprint const& f, ChunkKey l0) {
    return l0.x >= f.corner.x && l0.x < f.corner.x + f.span && l0.y >= f.corner.y && l0.y < f.corner.y + f.span &&
           l0.z >= f.corner.z && l0.z < f.corner.z + f.span;
}
bool overlaps(Footprint const& a, Footprint const& b) {
    for (int axis = 0; axis < 3; ++axis) {
        auto alo = axis == 0 ? a.corner.x : axis == 1 ? a.corner.y : a.corner.z;
        auto blo = axis == 0 ? b.corner.x : axis == 1 ? b.corner.y : b.corner.z;
        if (alo + a.span <= blo || blo + b.span <= alo) return false;
    }
    return true;
}

std::int64_t randomCoordinate(std::mt19937_64& rng, int trial) {
    auto magnitude = trial % 4 == 0 ? std::int64_t{1} << 61 : trial % 4 == 1 ? std::int64_t{1} << 40 : std::int64_t{4096};
    return std::uniform_int_distribution<std::int64_t>(-magnitude, magnitude)(rng);
}
}

int main() {
    std::mt19937_64 rng(20260906);
    std::size_t checkedPoints = 0, drawnTotal = 0;
    for (int trial = 0; trial < 1000; ++trial) {
        ChunkKey camera{randomCoordinate(rng, trial), std::int64_t(std::uniform_int_distribution<int>(-64, 300)(rng)),
                        randomCoordinate(rng, trial + 2), 0};
        auto targets = residencyTargets(camera, ShellRadius);
        std::set<ChunkKey> arriving{*keyAtLevel(camera, LevelCount - 1)};
        arriving.insert(targets.begin(), targets.begin() + 128);
        auto visible = drawnSet(camera, ShellRadius, [&](ChunkKey key) { return arriving.contains(key); });
        require(std::find(visible.begin(), visible.end(), camera) != visible.end(),
                "Near detail remained hidden behind its coarse ancestor after 128 loads");
        double keep = std::uniform_real_distribution<double>(0.0, 1.0)(rng);
        std::set<ChunkKey> resident;
        for (auto key : targets)
            if (std::bernoulli_distribution(trial % 5 == 0 ? 1.0 : keep)(rng)) resident.insert(key);
        for (int extra = 0; extra < 32; ++extra) {
            auto key = targets[std::uniform_int_distribution<std::size_t>(0, targets.size() - 1)(rng)];
            auto r = *region(camera, key.level, ShellRadius);
            key.x = r.hi.x + 1 + extra;
            resident.insert(key);
        }
        auto isResident = [&](ChunkKey k) { return resident.contains(k); };
        auto drawn = drawnSet(camera, ShellRadius, isResident);
        drawnTotal += drawn.size();
        std::vector<Footprint> footprints;
        for (auto key : drawn) {
            require(resident.contains(key), "drawn key is not resident");
            require(region(camera, key.level, ShellRadius)->contains(key), "drawn key outside its region");
            footprints.push_back(footprintOf(key));
        }
        for (std::size_t a = 0; a < footprints.size(); ++a)
            for (std::size_t b = a + 1; b < footprints.size(); ++b)
                require(!overlaps(footprints[a], footprints[b]), "drawn footprints overlap");
        auto top = *region(camera, LevelCount - 1, ShellRadius);
        auto topFootprint = footprintOf(top.lo);
        for (int sample = 0; sample < 64; ++sample) {
            auto topSpan = std::int64_t{1} << (LevelCount - 1);
            ChunkKey point{
                topFootprint.corner.x + std::uniform_int_distribution<std::int64_t>(0, (top.hi.x - top.lo.x + 1) * topSpan - 1)(rng),
                topFootprint.corner.y + std::uniform_int_distribution<std::int64_t>(0, (top.hi.y - top.lo.y + 1) * topSpan - 1)(rng),
                topFootprint.corner.z + std::uniform_int_distribution<std::int64_t>(0, (top.hi.z - top.lo.z + 1) * topSpan - 1)(rng), 0};
            bool covered = std::any_of(footprints.begin(), footprints.end(), [&](auto const& f) { return contains(f, point); });
            bool reachable = false;
            for (int level = 0; level < LevelCount && !reachable; ++level) {
                auto key = *keyAtLevel(point, level);
                reachable = resident.contains(key) && region(camera, level, ShellRadius)->contains(key);
            }
            require(covered == reachable, "hole disagrees with resident ancestor chain");
            ++checkedPoints;
        }
        if (trial % 5 == 0) {
            std::int64_t volume = 0;
            for (auto const& f : footprints) volume += f.span * f.span * f.span;
            auto topSpan = std::int64_t{1} << (LevelCount - 1);
            auto expected = (top.hi.x - top.lo.x + 1) * (top.hi.y - top.lo.y + 1) * (top.hi.z - top.lo.z + 1) * topSpan * topSpan * topSpan;
            require(volume == expected, "full residency does not partition the top region");
        }
        for (auto key : resident)
            if (!region(camera, key.level, ShellRadius)->contains(key))
                require(std::find(drawn.begin(), drawn.end(), key) == drawn.end(), "out-of-region resident drawn");
    }
    std::cout << "PASS frontier partition: 1000 anchors, " << checkedPoints << " points, " << drawnTotal << " drawn keys\n";
    return 0;
}
