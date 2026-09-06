#include "../world/Stream.hpp"
#include <chrono>
#include <iostream>
#include <thread>

namespace {
using namespace world;
void require(bool value, char const* message) {
    if (!value) throw std::runtime_error(message);
}
template<class Predicate>
void waitFor(Predicate predicate, Stream& stream) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (!predicate()) {
        auto status = stream.status();
        if (!status.error.empty()) throw std::runtime_error(status.error);
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("World worker timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
Stream::Reply next(Stream& stream) {
    std::optional<Stream::Reply> reply;
    waitFor([&] { reply = stream.poll(); return reply.has_value(); }, stream);
    return std::move(*reply);
}
bool hasCrystal(Chunk const& chunk) {
    if (!chunk.data) return (chunk.uniform & MaterialMask) == Crystal;
    for (auto material : *chunk.data) if ((material & MaterialMask) == Crystal) return true;
    return false;
}
WorldPosition metres(glm::dvec3 position) { return *normalizedPosition({0, 0, 0, 0}, position); }
// Level-0 row whose level-6 ancestor lies entirely above TerrainCeiling, so every ancestor above OverlayLevels is trivially air.
constexpr std::int64_t Sky = 256;
}

int main(int argc, char** argv) try {
    require(argc == 2, "Pass a new scratch world directory");
    std::filesystem::path path = argv[1];
    require(!std::filesystem::exists(path), "Smoke needs a new scratch directory; existing worlds are never removed");
    {
        Stream stream(path, 17);
        waitFor([&] { return stream.status().ready; }, stream);
        stream.retarget({0, 5, 0});
        for (int x = 0; x < 4; ++x) require(stream.request({x, 5, 0}), "Initial load rejected");
        waitFor([&] { return stream.status().replies == 4; }, stream);
        stream.retarget({2, 5, 0});
        for (int x = 0; x < 4; ++x)
            require(next(stream).chunks[0].key == ChunkKey{x, 5, 0}, "Retarget discarded a still-needed reply");
        for (int x = 0; x < 4; ++x) require(stream.request({x, 5, 0}), "Retained load rejected");
        stream.retarget({-5, 5, -7});
        require(stream.request({-5, 5, -7}), "New camera load rejected");
        auto reply = next(stream);
        require(!reply.edit && reply.chunks.size() == 1 &&
                reply.chunks[0].key == ChunkKey{-5, 5, -7}, "Stale camera reply escaped retarget");
        require(!stream.request({0, 5, 0}), "Out-of-view request accepted");

        stream.retarget({0, Sky, 0});
        require(stream.request({0, Sky, 0}), "Sky load rejected");
        reply = next(stream);
        require(reply.chunks[0].key == ChunkKey{0, Sky, 0} && !reply.chunks[0].data && reply.chunks[0].uniform == Air,
                "Sky chunk was not collapsed to uniform air");
        stream.retarget({0, -100, 0});
        require(stream.request({0, -100, 0}), "Deep load rejected");
        reply = next(stream);
        require(reply.chunks[0].key == ChunkKey{0, -100, 0} && !reply.chunks[0].data && reply.chunks[0].uniform == Bedrock,
                "Deep chunk was not collapsed to uniform bedrock");

        Brush brush{metres({-0.125, 24.0, -0.125}), 0.5f, Crystal};
        require(stream.edit(brush), "Brush could not be queued");
        require(!stream.edit(brush), "Concurrent brush accepted");
        stream.retarget({3, Sky, 3});
        reply = next(stream);
        require(reply.edit && !reply.chunks.empty(), "Teleport discarded a durable brush reply");
        bool found = false;
        for (auto const& chunk : reply.chunks) {
            require(chunk.key.level == 0, "Edit reply carried a non-level-0 key");
            found = found || hasCrystal(chunk);
        }
        require(found, "Durable brush did not return changed voxels");
        require(!stream.status().editing, "Consumed brush still blocks edits");

        ChunkKey sky{3, Sky, 3};
        require(stream.edit({{sky, glm::dvec3(4.0)}, 0.5f, Crystal}), "Sky brush could not be queued");
        reply = next(stream);
        require(reply.edit && reply.chunks.size() == 1 && reply.chunks[0].key == sky && hasCrystal(reply.chunks[0]),
                "Sky brush did not edit its chunk");
        for (int level = 1; level <= OverlayLevels; ++level) {
            auto coarse = *keyAtLevel(sky, level);
            require(stream.request(coarse), "Coarse request rejected");
            reply = next(stream);
            require(reply.chunks.size() == 1 && reply.chunks[0].key == coarse, "Coarse reply key mismatch");
            require(reply.chunks[0].data != nullptr && hasCrystal(reply.chunks[0]), "Saved edit not overlaid at level 1..3");
        }
        for (int level = OverlayLevels + 1; level <= 6; ++level) {
            auto coarse = *keyAtLevel(sky, level);
            require(stream.request(coarse), "Coarse request rejected");
            reply = next(stream);
            require(reply.chunks.size() == 1 && reply.chunks[0].key == coarse, "Coarse reply key mismatch");
            require(!reply.chunks[0].data && reply.chunks[0].uniform == Air, "Level above OverlayLevels was overlaid or not collapsed");
        }
        auto top = *keyAtLevel(sky, LevelCount - 1);
        require(stream.request(top), "Top-level request rejected");
        reply = next(stream);
        require(reply.chunks[0].key == top && !hasCrystal(reply.chunks[0]), "Top level chunk was overlaid");
        require(stream.request(sky), "Edited level-0 request rejected");
        reply = next(stream);
        require(reply.chunks[0].data != nullptr && hasCrystal(reply.chunks[0]), "Edited level-0 chunk lost its edit");

        stream.retarget(sky);
        // Data replies fill the in-flight bound; one more blocks the worker so the queue stays full.
        for (int i = 0; i < int(Stream::DataRepliesInFlight) + 1; ++i) require(stream.request(sky), "Data request rejected");
        waitFor([&] { return stream.status().replies == Stream::DataRepliesInFlight; }, stream);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        require(stream.status().replies == Stream::DataRepliesInFlight && stream.status().queued == 0, "Data replies exceeded in-flight bound");
        for (int i = 0; i < int(Stream::RequestCapacity) - 1; ++i)
            require(stream.request({i % 8, Sky + 1, i / 8}), "Request queue below capacity rejected");
        require(!stream.request({0, Sky + 1, 0}), "Terrain loads consumed the brush queue slot");
        require(stream.edit({{sky, glm::dvec3(4.0)}, 0.5f, Stone}), "Full terrain queue starved a brush");
        for (int i = 0; i < int(Stream::DataRepliesInFlight); ++i) {
            auto ready = stream.poll();
            require(ready && ready->chunks[0].key == sky && ready->chunks[0].data, "Data reply missing");
        }
        waitFor([&] { return stream.status().replies == Stream::RequestCapacity + 1; }, stream);
        require(stream.poll()->chunks[0].key == sky, "Blocked data reply not released");
        auto priorityEdit = next(stream);
        require(priorityEdit.edit, "Brush did not precede queued terrain loads");
        for (std::size_t i = 0; i < Stream::RequestCapacity - 1; ++i) {
            auto ready = stream.poll();
            require(ready && !ready->chunks[0].data &&
                    ready->chunks[0].key == ChunkKey{std::int64_t(i % 8), Sky + 1, std::int64_t(i / 8)},
                    "Uniform reply burst lost order or data");
        }
        require(!stream.poll(), "Reply queue not drained");

        for (int x = 0; x < 1000; ++x) {
            stream.retarget({x, 5, 0});
            require(stream.request({x, 5, 0}), "Travel request rejected");
            reply = next(stream);
            auto state = stream.status();
            require(state.queued <= Stream::RequestCapacity && state.replies <= Stream::DataRepliesInFlight &&
                    state.cachedBytes <= 64 * 1024 * 1024 && state.cachedChunks <= 2048,
                    "Travel exceeded worker or encoded cache budget");
        }
        require(stream.edit({metres({0.5, 24.5, 0.5}), 0.5f, Stone}), "Close brush could not be queued");
    }
    {
        Store store(path, 17);
        auto chunk = store.load({0, 3, 0});
        require(chunk[index(2, 2, 2)] == Stone, "Worker shutdown lost queued brush");
        auto edited = store.load({-1, 3, -1});
        bool found = false;
        for (auto material : edited) if ((material & MaterialMask) == Crystal) found = true;
        require(found, "Teleport brush did not survive restart");
        require(store.savedKeysWithin(*keyAtLevel({3, Sky, 3}, 3)) == std::vector<ChunkKey>{{3, Sky, 3}},
                "Sky edit missing from saved keys after restart");
    }
    std::cout << "PASS: retained loads survive retarget, stale loads rejected, uniform collapse, overlays, "
                 "brush priority, edits survive teleport and shutdown, 1000-chunk worker/cache bounds\n";
    return 0;
} catch (std::exception const& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
}
