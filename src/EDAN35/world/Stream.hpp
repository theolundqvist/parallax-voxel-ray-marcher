#pragma once

#include "Chunk.hpp"
#include "Frontier.hpp"
#include "Generate.hpp"
#include "Store.hpp"
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#if defined(__linux__)
#include <sys/resource.h>
#elif defined(__APPLE__)
#include <pthread.h>
#endif

namespace world {
class Stream {
public:
    static constexpr std::size_t RequestCapacity = 64;
    // One frame can ingest FrameUploadBudget of bricks, so the worker may run that far ahead.
    static constexpr std::size_t DataRepliesInFlight = FrameUploadBudget / sizeof(ChunkData);
    struct Reply {
        bool edit = false;
        std::vector<Chunk> chunks;
    };
    struct Status {
        bool ready = false;
        bool editing = false;
        std::uint64_t seed = 0;
        std::size_t queued = 0;
        std::size_t replies = 0;
        std::size_t cachedBytes = 0;
        std::size_t cachedChunks = 0;
        std::string error;
    };

    Stream(std::filesystem::path path, std::optional<std::uint64_t> seed)
        : worker([this, path = std::move(path), seed] { run(path, seed); }) {}
    Stream(Stream const&) = delete;
    Stream& operator=(Stream const&) = delete;
    ~Stream() {
        {
            std::lock_guard lock(mutex);
            stopping = true;
            requests.erase(std::remove_if(requests.begin(), requests.end(),
                [](Request const& request) { return !request.brush; }), requests.end());
        }
        changed.notify_all();
        worker.join();
    }

    Status status() const {
        std::lock_guard lock(mutex);
        auto result = state;
        result.queued = requests.size();
        result.replies = replies.size();
        return result;
    }
    void retarget(ChunkKey camera) {
        std::lock_guard lock(mutex);
        center = camera;
        if (loading && !wanted(*loading)) loadRequested = false;
        requests.erase(std::remove_if(requests.begin(), requests.end(),
            [this](Request const& request) { return !request.brush && !wanted(request.key); }), requests.end());
        replies.erase(std::remove_if(replies.begin(), replies.end(),
            [this](Reply const& reply) { return !reply.edit && !wanted(reply.chunks.front().key); }), replies.end());
        dataReplies = countDataReplies();
        changed.notify_all();
    }
    bool request(ChunkKey key) {
        std::lock_guard lock(mutex);
        if (!state.ready || !state.error.empty() || stopping || !wanted(key)) return false;
        if (loading == key && !loadRequested) {
            loadRequested = true;
            return true;
        }
        // Terrain loads must leave a queue slot for the next brush.
        if (requests.size() >= RequestCapacity - 1) return false;
        requests.push_back({key, std::nullopt});
        changed.notify_one();
        return true;
    }
    bool edit(Brush brush) {
        std::lock_guard lock(mutex);
        if (!state.ready || state.editing || !state.error.empty() || stopping ||
            requests.size() >= RequestCapacity)
            return false;
        state.editing = true;
        requests.push_front({{}, brush});
        changed.notify_one();
        return true;
    }
    std::optional<Reply> poll() {
        std::lock_guard lock(mutex);
        if (replies.empty()) return std::nullopt;
        Reply reply = std::move(replies.front());
        replies.pop_front();
        if (reply.edit) state.editing = false;
        if (carriesData(reply)) --dataReplies;
        changed.notify_one();
        return reply;
    }

private:
    struct Request {
        ChunkKey key;
        std::optional<Brush> brush;
    };
    mutable std::mutex mutex;
    std::condition_variable changed;
    std::deque<Request> requests;
    std::deque<Reply> replies;
    Status state;
    std::optional<ChunkKey> center;
    std::optional<ChunkKey> loading;
    bool loadRequested = false;
    std::size_t dataReplies = 0;
    bool stopping = false;
    std::thread worker;

    bool wanted(ChunkKey key) const {
        if (!center) return false;
        auto view = region(*center, key.level, ShellRadius);
        return view && view->contains(key);
    }
    static bool carriesData(Reply const& reply) {
        return std::any_of(reply.chunks.begin(), reply.chunks.end(), [](Chunk const& c) { return c.data != nullptr; });
    }
    std::size_t countDataReplies() const {
        return std::size_t(std::count_if(replies.begin(), replies.end(), carriesData));
    }
    static void collapse(Chunk& chunk) {
        std::uint8_t value;
        if (chunk.data && isUniform(*chunk.data, value)) {
            chunk.uniform = value;
            chunk.data.reset();
        }
    }
    static Chunk produce(Store& store, ChunkKey key) {
        Chunk chunk{key, Air, nullptr};
        if (key.level == 0) {
            chunk.data = std::make_unique<ChunkData>(store.load(key));
            collapse(chunk);
            return chunk;
        }
        std::vector<ChunkKey> saved;
        if (key.level <= OverlayLevels) saved = store.savedKeysWithin(key);
        if (saved.empty())
            if (auto uniform = trivialUniform(store.seed(), key)) {
                chunk.uniform = *uniform;
                return chunk;
            }
        chunk.data = std::make_unique<ChunkData>(generateChunk(store.seed(), key));
        for (auto savedKey : saved) overlaySaved(*chunk.data, key, savedKey, store.load(savedKey));
        collapse(chunk);
        return chunk;
    }

    void run(std::filesystem::path const& path, std::optional<std::uint64_t> seed) {
        #if defined(__linux__)
        setpriority(PRIO_PROCESS, 0, 10);
        #elif defined(__APPLE__)
        pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
        #endif
        try {
            Store store(path, seed);
            {
                std::lock_guard lock(mutex);
                state.ready = true;
                state.seed = store.seed();
            }
            for (;;) {
                Request request;
                {
                    std::unique_lock lock(mutex);
                    changed.wait(lock, [this] { return stopping || !requests.empty(); });
                    if (requests.empty()) break;
                    request = requests.front();
                    requests.pop_front();
                    if (!request.brush) {
                        loading = request.key;
                        loadRequested = true;
                    }
                }
                Reply reply{request.brush.has_value(), {}};
                if (request.brush) {
                    reply.chunks = store.edit(*request.brush);
                    for (auto& chunk : reply.chunks) collapse(chunk);
                } else reply.chunks.push_back(produce(store, request.key));
                bool data = carriesData(reply);
                {
                    std::unique_lock lock(mutex);
                    state.cachedBytes = store.cachedBytes();
                    state.cachedChunks = store.cachedChunks();
                    changed.wait(lock, [this, &reply, data] {
                        return stopping || (!reply.edit && !loadRequested) || !data ||
                               dataReplies < DataRepliesInFlight;
                    });
                    if (!stopping && (reply.edit || loadRequested)) {
                        replies.push_back(std::move(reply));
                        dataReplies += data;
                    }
                    loading.reset();
                    loadRequested = false;
                }
            }
        } catch (std::exception const& error) {
            std::lock_guard lock(mutex);
            state.error = error.what();
            state.editing = false;
            requests.clear();
        }
    }
};
}
