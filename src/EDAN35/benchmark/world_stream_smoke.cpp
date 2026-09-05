#include "../world/Stream.hpp"
#include <chrono>
#include <iostream>
#include <thread>

namespace {
void require(bool value, char const* message) {
    if (!value) throw std::runtime_error(message);
}
template<class Predicate>
void waitFor(Predicate predicate, world::Stream& stream) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (!predicate()) {
        auto status = stream.status();
        if (!status.error.empty()) throw std::runtime_error(status.error);
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("World worker timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
}

int main(int argc, char** argv) try {
    require(argc == 2, "Pass a new scratch world directory");
    std::filesystem::path path = argv[1];
    require(!std::filesystem::exists(path), "Smoke needs a new scratch directory; existing worlds are never removed");
    {
        world::Stream stream(path, 17);
        waitFor([&] { return stream.status().ready; }, stream);
        stream.retarget(1);
        for (int x = 0; x < 8; ++x) stream.request({x, 5, 0}, 1);
        stream.retarget(2);
        require(stream.request({-5, 5, -7}, 2), "New camera load rejected");
        std::optional<world::Stream::Reply> reply;
        waitFor([&] { reply = stream.poll(); return reply.has_value(); }, stream);
        require(!reply->edit && reply->epoch == 2 && reply->chunks.size() == 1 &&
                reply->chunks[0].key == world::ChunkKey{-5, 5, -7}, "Stale camera reply escaped retarget");
        require(!stream.request({0, 5, 0}, 1), "Old camera request accepted");

        world::Brush brush{glm::vec3(-0.125f, 24.0f, -0.125f), 0.5f, world::Crystal};
        require(stream.edit(brush), "Brush could not be queued");
        require(!stream.edit(brush), "Concurrent brush accepted");
        stream.retarget(3);
        waitFor([&] { reply = stream.poll(); return reply.has_value(); }, stream);
        require(reply->edit && !reply->chunks.empty(), "Teleport discarded a durable brush reply");
        bool found = false;
        for (auto const& chunk : reply->chunks) for (auto material : chunk.data)
            if ((material & 15) == world::Crystal) found = true;
        require(found, "Durable brush did not return changed voxels");
        require(!stream.status().editing, "Consumed brush still blocks edits");
        for (int x = 0; x < 1000; ++x) {
            stream.retarget(4 + x);
            require(stream.request({x, 5, 0}, 4 + x), "Travel request rejected");
            waitFor([&] { reply = stream.poll(); return reply.has_value(); }, stream);
            auto state = stream.status();
            require(state.queued <= 8 && state.replies <= 2 && state.cachedBytes <= 64 * 1024 * 1024 &&
                    state.cachedChunks <= 2048, "Travel exceeded worker or encoded cache budget");
        }
        require(stream.edit({glm::vec3(0.5f, 24.5f, 0.5f), 0.5f, world::Stone}), "Close brush could not be queued");
    }
    {
        world::Store store(path, 17);
        auto chunk = store.load({0, 3, 0});
        require(chunk[world::index(2, 2, 2)] == world::Stone, "Worker shutdown lost queued brush");
        auto edited = store.load({-1, 3, -1});
        bool found = false;
        for (auto material : edited) if ((material & 15) == world::Crystal) found = true;
        require(found, "Teleport brush did not survive restart");
    }
    std::cout << "PASS: stale loads rejected, edits survive teleport and shutdown, 1000-chunk worker/cache bounds\n";
    return 0;
} catch (std::exception const& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
}
