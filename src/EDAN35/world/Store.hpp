#pragma once

#include "Chunk.hpp"
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace world {
// Worker-owned synchronous storage. Edit acknowledgments follow durable checkpoints.
// Errors identify FailedBeforeCommit, CommitUncertain, or DurableCheckpointBlocked;
// after an edit error the owner must destroy/reconstruct Store before further work.
class Store {
public:
    explicit Store(std::filesystem::path directory,
                   std::optional<std::uint64_t> requestedSeed = std::nullopt);
    ~Store();
    Store(Store const&) = delete;
    Store& operator=(Store const&) = delete;

    std::uint64_t seed() const;
    ChunkData load(ChunkKey key);
    std::vector<Chunk> edit(Brush brush);
    // Allocated encoded payload capacity; bounded separately from LRU metadata.
    std::size_t cachedBytes() const;
    std::size_t cachedChunks() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
