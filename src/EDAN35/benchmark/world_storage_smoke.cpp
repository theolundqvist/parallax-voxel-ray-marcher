#include "../world/Store.hpp"
#include "../world/Generate.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;
using namespace world;
constexpr std::uint64_t Seed = DefaultSeed;

void require(bool value, std::string const& message) {
    if (!value) throw std::runtime_error(message);
}
template<class Function> void fails(Function action, std::string const& text) {
    try { action(); }
    catch (std::exception const& error) {
        require(std::string(error.what()).find(text) != std::string::npos,
                "Expected '" + text + "', got '" + error.what() + "'");
        return;
    }
    throw std::runtime_error("Expected failure: " + text);
}
void put32(Bytes& bytes, std::uint32_t value) {
    for (int i = 0; i != 4; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
}
void put64(Bytes& bytes, std::uint64_t value) {
    put32(bytes, static_cast<std::uint32_t>(value));
    put32(bytes, static_cast<std::uint32_t>(value >> 32));
}
std::uint32_t get32(Bytes const& bytes, std::size_t at) {
    require(at + 4 <= bytes.size(), "Fixture read out of bounds");
    return bytes[at] | (std::uint32_t(bytes[at + 1]) << 8) |
           (std::uint32_t(bytes[at + 2]) << 16) | (std::uint32_t(bytes[at + 3]) << 24);
}
void set32(Bytes& bytes, std::size_t at, std::uint32_t value) {
    require(at + 4 <= bytes.size(), "Fixture write out of bounds");
    for (int i = 0; i != 4; ++i) bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8));
}
std::uint32_t checksum(std::span<const std::uint8_t> bytes) {
    static const auto table = [] {
        std::array<std::uint32_t, 256> result{};
        for (std::uint32_t i = 0; i != 256; ++i) {
            auto value = i;
            for (int bit = 0; bit != 8; ++bit)
                value = (value & 1) ? (value >> 1) ^ 0xedb88320u : value >> 1;
            result[i] = value;
        }
        return result;
    }();
    auto value = 0xffffffffu;
    for (auto byte : bytes) value = table[(value ^ byte) & 255] ^ (value >> 8);
    return value ^ 0xffffffffu;
}
void seal(Bytes& bytes) { put32(bytes, checksum(bytes)); }
void reseal(Bytes& bytes) { set32(bytes, bytes.size() - 4, checksum(std::span(bytes).first(bytes.size() - 4))); }
void magic(Bytes& bytes, char const* value) { bytes.insert(bytes.end(), value, value + 8); }
void write(fs::path const& path, Bytes const& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    file.close();
}
Bytes read(fs::path const& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    file.exceptions(std::ios::badbit | std::ios::failbit);
    auto size = file.tellg();
    require(size >= 0 && size < 1024 * 1024, "Unexpected fixture size");
    Bytes bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return bytes;
}
fs::path snapshot(fs::path const& directory, ChunkKey key) {
    return directory / ("chunk_" + std::to_string(key.x) + "_" + std::to_string(key.y) + "_" +
                        std::to_string(key.z) + ".bin");
}
// Independent on-disk fixtures, not production fault hooks. All multibyte fields are LE.
Bytes record(ChunkKey key, ChunkData const& data, std::uint32_t encoding) {
    Bytes payload;
    if (encoding == 0) payload.push_back(data[0]);
    else if (encoding == 2) payload.assign(data.begin(), data.end());
    else {
        for (std::size_t start = 0; start < data.size();) {
            auto end = start + 1;
            while (end < data.size() && data[start] == data[end]) ++end;
            payload.push_back(static_cast<std::uint8_t>(end - start));
            payload.push_back(static_cast<std::uint8_t>((end - start) >> 8));
            payload.push_back(data[start]);
            start = end;
        }
    }
    Bytes bytes;
    bytes.reserve(52 + payload.size());
    magic(bytes, "FWCHNK01");
    put32(bytes, 1);
    put32(bytes, GeneratorVersion);
    put64(bytes, Seed);
    put32(bytes, static_cast<std::uint32_t>(key.x));
    put32(bytes, static_cast<std::uint32_t>(key.y));
    put32(bytes, static_cast<std::uint32_t>(key.z));
    put32(bytes, encoding);
    put32(bytes, static_cast<std::uint32_t>(data.size()));
    put32(bytes, static_cast<std::uint32_t>(payload.size()));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    seal(bytes);
    return bytes;
}
Bytes journal(std::vector<Bytes> const& records) {
    Bytes bytes;
    magic(bytes, "FWTXN001");
    put32(bytes, 1);
    put32(bytes, GeneratorVersion);
    put64(bytes, Seed);
    put32(bytes, static_cast<std::uint32_t>(records.size()));
    std::size_t size = 0;
    for (auto const& entry : records) size += 4 + entry.size();
    put32(bytes, static_cast<std::uint32_t>(size));
    for (auto const& entry : records) {
        put32(bytes, static_cast<std::uint32_t>(entry.size()));
        bytes.insert(bytes.end(), entry.begin(), entry.end());
    }
    seal(bytes);
    return bytes;
}
void initialize(fs::path const& directory) { Store store(directory, Seed); }
void checkBounds(Store const& store) {
    require(store.cachedBytes() <= 64 * 1024 * 1024, "Cache exceeded allocated-payload bound");
    require(store.cachedChunks() <= 2048, "Cache exceeded entry/metadata bound");
}

void manifestAndLock(fs::path const& root) {
    auto directory = root / "manifest";
    {
        Store store(directory, Seed);
        require(store.seed() == Seed, "Seed was not persisted");
        fails([&] { Store duplicate(directory); }, "WorldInUse");
    }
    { Store store(directory); require(store.seed() == Seed, "Implicit reopen changed seed"); }
    fails([&] { Store store(directory, Seed + 1); }, "seed");
    auto original = read(directory / "manifest.bin");
    auto broken = original;
    set32(broken, 12, GeneratorVersion + 1);
    reseal(broken);
    write(directory / "manifest.bin", broken);
    fails([&] { Store store(directory); }, "Unsupported");
    write(directory / "manifest.bin", original);
    { Store store(directory); } // Failed constructors released the actual lock.
    fs::create_directories(root / "orphan");
    write(root / "orphan" / "unrelated", Bytes{1});
    fails([&] { Store store(root / "orphan"); }, "Missing manifest");
    std::cout << "PASS manifest seed/version, exclusive writer, failed-open lock release\n";
}

void negativeBrushAndAir(fs::path const& root) {
    auto directory = root / "brush";
    Brush brush{glm::vec3(-8.f, 0.f, -8.f), .25f, 0x55};
    std::vector<Chunk> edited;
    {
        Store store(directory, Seed);
        edited = store.edit(brush);
        require(edited.size() == 8, "Negative corner brush did not edit eight chunks");
        for (auto const& chunk : edited) {
            require(chunk.key.x == -2 || chunk.key.x == -1, "Negative X floor ownership wrong");
            require(chunk.key.y == -1 || chunk.key.y == 0, "Y floor ownership wrong");
            require(chunk.key.z == -2 || chunk.key.z == -1, "Negative Z floor ownership wrong");
            auto before = generateChunk(Seed, chunk.key);
            auto expected = before;
            int x = chunk.key.x == -2 ? 31 : 0;
            int y = chunk.key.y == -1 ? 31 : 0;
            int z = chunk.key.z == -2 ? 31 : 0;
            expected[index(x, y, z)] = brush.material;
            require(chunk.data == expected, "Brush changed voxels outside center-sampled sphere");
            require(store.load(chunk.key) == chunk.data, "Acknowledged edit was not readable");
        }
        require(!fs::exists(directory / "pending.txn"), "Successful edit retained pending journal");
    }
    { Store store(directory); for (auto const& chunk : edited)
        require(store.load(chunk.key) == chunk.data, "Restart lost negative cross-chunk edit"); }

    ChunkKey key{0, 0, 0};
    ChunkData empty{};
    auto generated = generateChunk(Seed, key);
    require(std::any_of(generated.begin(), generated.end(), [](auto v) { return v != Air; }),
            "All-air persistence fixture must replace generated solid terrain");
    write(snapshot(directory, key), record(key, empty, 0));
    {
        Store store(directory);
        require(store.load(key) == empty, "Saved air regenerated");
        Brush one{glm::vec3(1.125f), .25f, Crystal};
        auto added = store.edit(one);
        require(added.size() == 1 && added[0].data[index(4, 4, 4)] == Crystal, "Interior brush insertion failed");
        one.material = Air;
        auto erased = store.edit(one);
        require(erased.size() == 1 && erased[0].data == empty, "Last solid brush did not erase to air");
    }
    { Store store(directory); require(store.load(key) == empty, "All-air edit lost on restart"); }
    auto saved = read(snapshot(directory, key));
    require(get32(saved, 36) == 0 && saved.size() == 53, "All-air snapshot is not explicit uniform encoding");
    std::cout << "PASS negative eight-chunk center sampling, tint, durable restart, edited all-air snapshot\n";
}

void codecsAndCorruption(fs::path const& root) {
    auto directory = root / "codec";
    initialize(directory);
    ChunkData uniform, rle, raw;
    uniform.fill(0x23);
    for (std::size_t i = 0; i < raw.size(); ++i) {
        rle[i] = i < 16384 ? Grass : Stone;
        raw[i] = static_cast<std::uint8_t>((i % 5) + 1 + ((i % 16) << 4));
    }
    std::array<ChunkData const*, 3> values{&uniform, &rle, &raw};
    for (int code = 0; code != 3; ++code) {
        ChunkKey key{10 + code, 0, 0};
        write(snapshot(directory, key), record(key, *values[code], code));
    }
    {
        Store store(directory);
        for (int code = 0; code != 3; ++code)
            require(store.load({10 + code, 0, 0}) == *values[code], "Fixture codec roundtrip mismatch");
        // Actual encoder rewrites edited full snapshots; the same pattern must retain RLE/raw choice.
        for (int code = 1; code != 3; ++code) {
            ChunkKey key{10 + code, 0, 0};
            auto changes = store.edit({origin(key) + glm::vec3(1.125f), .25f, Crystal});
            require(changes.size() == 1, "Codec edit did not change fixture");
            auto expected = *values[code];
            for (int z = 3; z <= 5; ++z) for (int y = 3; y <= 5; ++y) for (int x = 3; x <= 5; ++x)
                if ((x - 4) * (x - 4) + (y - 4) * (y - 4) + (z - 4) * (z - 4) <= 1)
                    expected[index(x, y, z)] = Crystal;
            require(changes[0].data == expected && store.load(key) == expected, "Encoded edit lost data");
            require(get32(read(snapshot(directory, key)), 36) == std::uint32_t(code), "Encoder chose nonminimal codec");
        }
    }
    {
        // Fixtures mutate files only with Store closed; corrupt disk reads must never regenerate.
        auto load = [&](ChunkKey key) { Store store(directory); return store.load(key); };
        ChunkKey key{10, 0, 0};
        auto original = read(snapshot(directory, key));
        auto broken = original;
        broken[48] ^= 1;
        write(snapshot(directory, key), broken);
        fails([&] { load(key); }, "checksum");
        broken = original;
        set32(broken, 44, 0xffffffffu);
        reseal(broken);
        write(snapshot(directory, key), broken);
        fails([&] { load(key); }, "payload length");
        broken = original;
        broken[48] = 0x10; // A tinted air byte is invalid, not empty.
        reseal(broken);
        write(snapshot(directory, key), broken);
        fails([&] { load(key); }, "material");
        broken = original;
        set32(broken, 24, 999);
        reseal(broken);
        write(snapshot(directory, key), broken);
        fails([&] { load(key); }, "filename/key");
        write(snapshot(directory, key), original);
        fs::resize_file(snapshot(directory, key), 1024 * 1024 * 1024);
        fails([&] { load(key); }, "length");
        write(snapshot(directory, key), original);
        fs::resize_file(snapshot(directory, key), 12);
        fails([&] { load(key); }, "length");
        write(snapshot(directory, key), original);
        auto malformedRle = record({11, 0, 0}, rle, 1);
        malformedRle[48] = malformedRle[49] = 0;
        reseal(malformedRle);
        write(snapshot(directory, {11, 0, 0}), malformedRle);
        fails([&] { load({11, 0, 0}); }, "RLE run");
        fs::remove(snapshot(directory, key));
        fs::create_directory(snapshot(directory, key));
        fails([&] { load(key); }, "type/length");
    }
    std::cout << "PASS uniform/RLE/raw codec, encoder selection, corruption, checksum/identity/material/length bounds\n";
}

void failureAndRecovery(fs::path const& root) {
    auto directory = root / "failure";
    initialize(directory);
    Brush brush{glm::vec3(-8.f, 0.f, -8.f), .25f, 0x55};
    std::array<ChunkKey, 8> keys{};
    std::size_t keyCount = 0;
    for (int z = -2; z <= -1; ++z) for (int y = -1; y <= 0; ++y) for (int x = -2; x <= -1; ++x)
        keys[keyCount++] = {x, y, z};
    fs::create_directory(directory / "pending.txn.tmp");
    {
        Store store(directory);
        fails([&] { store.edit(brush); }, "FailedBeforeCommit");
        fails([&] { store.load(keys[0]); }, "StorageBlocked");
        require(!fs::exists(directory / "pending.txn"), "Precommit error published journal");
        for (auto key : keys) require(!fs::exists(snapshot(directory, key)), "Precommit error checkpointed data");
    }
    fs::remove(directory / "pending.txn.tmp");
    auto blocker = snapshot(directory, keys.back());
    blocker += ".tmp";
    fs::create_directory(blocker);
    Bytes pending;
    {
        Store store(directory);
        fails([&] { store.edit(brush); }, "DurableCheckpointBlocked");
        require(fs::exists(directory / "pending.txn"), "Checkpoint failure discarded committed journal");
        require(fs::exists(snapshot(directory, keys.front())), "Fixture did not exercise partial checkpoints");
        require(!fs::exists(snapshot(directory, keys.back())), "Blocked final checkpoint unexpectedly succeeded");
        pending = read(directory / "pending.txn");
        fails([&] { store.edit(brush); }, "StorageBlocked");
    }
    fails([&] { Store store(directory); }, "DurableCheckpointBlocked");
    require(read(directory / "pending.txn") == pending, "Failed recovery modified pending journal");
    fs::remove(blocker);
    std::array<ChunkData, 8> recovered;
    {
        Store store(directory);
        for (std::size_t i = 0; i < keys.size(); ++i) {
            auto key = keys[i];
            auto expected = generateChunk(Seed, key);
            expected[index(key.x == -2 ? 31 : 0, key.y == -1 ? 31 : 0, key.z == -2 ? 31 : 0)] = brush.material;
            recovered[i] = store.load(key);
            require(recovered[i] == expected, "Recovery did not restore every brush post-image");
        }
        require(!fs::exists(directory / "pending.txn"), "Recovery did not remove journal");
    }
    write(directory / "pending.txn", pending); // Simulates replay after durable checkpoint but before unlink.
    { Store store(directory); for (std::size_t i = 0; i < keys.size(); ++i)
        require(store.load(keys[i]) == recovered[i], "Journal replay is not idempotent"); }
    auto corrupt = pending;
    corrupt.back() ^= 1;
    write(directory / "pending.txn", corrupt);
    fails([&] { Store store(directory); }, "checksum");
    require(read(directory / "pending.txn") == corrupt, "Corrupt committed journal was discarded");
    auto duplicated = journal({record(keys[0], recovered[0], 2), record(keys[0], recovered[0], 2)});
    write(directory / "pending.txn", duplicated);
    auto before = read(snapshot(directory, keys[0]));
    fails([&] { Store store(directory); }, "Duplicate");
    require(read(snapshot(directory, keys[0])) == before, "Malformed journal checkpointed before complete validation");
    fs::resize_file(directory / "pending.txn", 1024 * 1024 * 1024);
    fails([&] { Store store(directory); }, "length");
    fs::remove(directory / "pending.txn");
    std::cout << "PASS before-commit refusal, poisoned instance, real checkpoint failure, full recovery, idempotent replay, bounded corrupt journals\n";
}

void cacheTravel(fs::path const& root) {
    auto directory = root / "travel";
    Store store(directory, Seed);
    auto start = std::chrono::steady_clock::now();
    std::uint64_t fingerprint = 14695981039346656037ull;
    for (int i = 0; i != 1000; ++i) {
        ChunkKey key{i - 500, (i % (MaxChunkY - MinChunkY + 1)) + MinChunkY, 137};
        auto data = store.load(key);
        require(data == generateChunk(Seed, key), "Travel generation differs from pure generator");
        for (auto byte : data) {
            fingerprint ^= byte;
            fingerprint *= 1099511628211ull;
        }
        checkBounds(store);
    }
    // Generator v1 is a persisted-world compatibility contract. FMA contraction on
    // ARM changed boundary voxels without changing encoded sizes; this catches it.
    require(GeneratorVersion == 1 && fingerprint == 0x1382c1d279ff8167ull,
            "Generator v1 raw-byte portability fingerprint changed");
    std::cout << "PASS generator v1 1000-chunk raw-byte fingerprint: " << std::hex
              << fingerprint << std::dec << '\n';
    auto milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::cout << "PASS 1000 generated-chunk travel: " << milliseconds << " ms, "
              << store.cachedBytes() << " allocated encoded bytes, " << store.cachedChunks() << " entries\n";
    ChunkData raw;
    for (std::size_t i = 0; i < raw.size(); ++i) raw[i] = static_cast<std::uint8_t>(1 + i % 5);
    // Exercise BOTH caps at the actual limit, not just a small-map upper-bound assertion.
    for (int i = 0; i != 2050; ++i) {
        ChunkKey key{i, MaxChunkY, 500};
        write(snapshot(directory, key), record(key, raw, 2));
        require(store.load(key) == raw, "Raw travel cache changed snapshot");
        checkBounds(store);
    }
    require(store.cachedChunks() == 2048 && store.cachedBytes() == 64 * 1024 * 1024,
            "Worst-case raw cache did not hit and hold both limits");
    require(store.load({0, MaxChunkY, 500}) == raw, "Evicted saved snapshot did not reload");
    checkBounds(store);
    std::cout << "PASS 2050 raw-snapshot travel: LRU holds 2048 entries / 64 MiB; evicted edit remains authoritative\n";
}
}

int main() {
#ifdef _WIN32
    std::cerr << "World storage smoke requires Linux/macOS durable filesystem support\n";
    return 77;
#else
    fs::path root;
    try {
        auto base = fs::temp_directory_path();
        auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt != 100; ++attempt) {
            auto candidate = base / ("floating-world-storage-" + std::to_string(stamp) + "-" + std::to_string(attempt));
            if (fs::create_directory(candidate)) { root = candidate; break; }
        }
        require(!root.empty(), "Could not create unique smoke directory");
        manifestAndLock(root);
        negativeBrushAndAir(root);
        codecsAndCorruption(root);
        failureAndRecovery(root);
        cacheTravel(root);
        fs::remove_all(root);
        std::cout << "PASS world storage smoke (real Store + generator + filesystem; no GL)\n";
        return 0;
    } catch (std::exception const& error) {
        std::cerr << "FAIL: " << error.what() << "\nPreserved fixtures: " << root << '\n';
        return 1;
    }
#endif
}
