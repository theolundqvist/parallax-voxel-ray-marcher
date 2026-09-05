#pragma once

#include "Chunk.hpp"
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
    struct Reply {
        std::uint64_t epoch = 0;
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
    void retarget(std::uint64_t epoch) {
        std::lock_guard lock(mutex);
        currentEpoch = epoch;
        requests.erase(std::remove_if(requests.begin(), requests.end(),
            [](Request const& request) { return !request.brush; }), requests.end());
        replies.erase(std::remove_if(replies.begin(), replies.end(),
            [](Reply const& reply) { return !reply.edit; }), replies.end());
        changed.notify_all();
    }
    bool request(ChunkKey key, std::uint64_t epoch) {
        std::lock_guard lock(mutex);
        if (!state.ready || !state.error.empty() || stopping || requests.size() >= 8 || epoch != currentEpoch)
            return false;
        requests.push_back({key, epoch, std::nullopt});
        changed.notify_one();
        return true;
    }
    bool edit(Brush brush) {
        std::lock_guard lock(mutex);
        if (!state.ready || state.editing || !state.error.empty() || stopping || requests.size() >= 8)
            return false;
        state.editing = true;
        requests.push_front({{}, currentEpoch, brush});
        changed.notify_one();
        return true;
    }
    std::optional<Reply> poll() {
        std::lock_guard lock(mutex);
        if (replies.empty()) return std::nullopt;
        Reply reply = std::move(replies.front());
        replies.pop_front();
        if (reply.edit) state.editing = false;
        changed.notify_one();
        return reply;
    }

private:
    struct Request {
        ChunkKey key;
        std::uint64_t epoch;
        std::optional<Brush> brush;
    };
    mutable std::mutex mutex;
    std::condition_variable changed;
    std::deque<Request> requests;
    std::deque<Reply> replies;
    Status state;
    std::uint64_t currentEpoch = 0;
    bool stopping = false;
    std::thread worker;

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
                }
                Reply reply{request.epoch, request.brush.has_value(), {}};
                if (request.brush) reply.chunks = store.edit(*request.brush);
                else reply.chunks.push_back({request.key, store.load(request.key)});
                {
                    std::unique_lock lock(mutex);
                    state.cachedBytes = store.cachedBytes();
                    state.cachedChunks = store.cachedChunks();
                    changed.wait(lock, [this, &reply] {
                        return stopping || (!reply.edit && reply.epoch != currentEpoch) || replies.size() < 2;
                    });
                    if (!stopping && (reply.edit || reply.epoch == currentEpoch)) replies.push_back(std::move(reply));
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
