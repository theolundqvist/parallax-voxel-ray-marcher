#include "../world/Store.hpp"
#include "../world/Generate.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;
using namespace world;
constexpr std::uint64_t Seed = DefaultSeed;
constexpr std::int64_t Big = std::int64_t{1} << 62;
constexpr std::int64_t Fine = (std::int64_t{1} << 53) + 1;
constexpr std::size_t Header = 60;

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
std::map<std::string, Bytes> readAll(fs::path const& directory) {
    std::map<std::string, Bytes> contents;
    for (auto const& entry : fs::directory_iterator(directory))
        contents[entry.path().filename().string()] = read(entry.path());
    return contents;
}
fs::path snapshot(fs::path const& directory, ChunkKey key) {
    return directory / ("chunk_" + std::to_string(key.x) + "_" + std::to_string(key.y) + "_" +
                        std::to_string(key.z) + ".bin");
}
WorldPosition at(ChunkKey anchor, glm::dvec3 offset) { return {anchor, offset}; }
WorldPosition metres(glm::dvec3 position) { return *normalizedPosition({0, 0, 0, 0}, position); }
// A level-0 key whose generated chunk is solid terrain at the given column.
ChunkKey ground(std::int64_t x, std::int64_t z) {
    auto height = terrainHeight(Seed, double(x) * ChunkSpan + 4.0, double(z) * ChunkSpan + 4.0);
    return {x, std::int64_t(std::floor(height / ChunkSpan)) - 1, z, 0};
}
// Independent on-disk fixtures, not production fault hooks. All multibyte fields are LE.
Bytes record(ChunkKey key, ChunkData const& data, std::uint32_t encoding, std::uint32_t version = GeneratorVersion) {
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
    bytes.reserve(Header + 4 + payload.size());
    magic(bytes, "FWCHNK01");
    put32(bytes, 2);
    put32(bytes, version);
    put64(bytes, Seed);
    put64(bytes, static_cast<std::uint64_t>(key.x));
    put64(bytes, static_cast<std::uint64_t>(key.y));
    put64(bytes, static_cast<std::uint64_t>(key.z));
    put32(bytes, key.level);
    put32(bytes, encoding);
    put32(bytes, static_cast<std::uint32_t>(payload.size()));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    seal(bytes);
    return bytes;
}
Bytes journal(std::vector<Bytes> const& records, std::uint32_t version = GeneratorVersion) {
    Bytes bytes;
    magic(bytes, "FWTXN001");
    put32(bytes, 2);
    put32(bytes, version);
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
Bytes islandsManifest() {
    Bytes bytes;
    magic(bytes, "FWORLD01");
    put32(bytes, 1);
    put32(bytes, 1);
    put32(bytes, ChunkSize);
    put32(bytes, 1);
    put32(bytes, 1);
    put32(bytes, 4);
    put64(bytes, 20251205);
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
    require(get32(original, 8) == 2 && get32(original, 12) == GeneratorVersion, "Manifest is not format 2 / current generator");
    auto broken = original;
    set32(broken, 12, GeneratorVersion + 1);
    reseal(broken);
    write(directory / "manifest.bin", broken);
    fails([&] { Store store(directory); }, "Unsupported world format/generator");
    write(directory / "manifest.bin", original);
    { Store store(directory); } // Failed constructors released the actual lock.
    fs::create_directories(root / "orphan");
    write(root / "orphan" / "unrelated", Bytes{1});
    fails([&] { Store store(root / "orphan"); }, "Missing manifest");

    auto islands = root / "islands";
    fs::create_directories(islands);
    write(islands / "manifest.bin", islandsManifest());
    write(islands / "world.lock", Bytes{});
    ChunkData solid;
    solid.fill(Stone);
    Bytes v1;
    magic(v1, "FWCHNK01");
    put32(v1, 1);
    put32(v1, 1);
    put64(v1, 20251205);
    put32(v1, 3); put32(v1, 0); put32(v1, 3);
    put32(v1, 0);
    put32(v1, static_cast<std::uint32_t>(solid.size()));
    put32(v1, 1);
    v1.push_back(Stone);
    seal(v1);
    write(islands / "chunk_3_0_3.bin", v1);
    write(islands / "pending.txn", Bytes{1, 2, 3});
    auto before = readAll(islands);
    fails([&] { Store store(islands); }, "Unsupported world format/generator");
    fails([&] { Store store(islands, Seed); }, "Unsupported world format/generator");
    require(readAll(islands) == before, "Rejected islands directory was mutated");
    std::cout << "PASS manifest seed/version, exclusive writer, failed-open lock release, v1 islands rejected untouched\n";
}

void waterMigration(fs::path const& root) {
    auto legacyWorld = [&](fs::path const& directory) {
        initialize(directory);
        auto manifest = read(directory / "manifest.bin");
        set32(manifest, 12, 4);
        reseal(manifest);
        write(directory / "manifest.bin", manifest);
    };
    // Use real coastline terrain so both carved solid Air and unchanged ocean Air exist.
    ChunkKey key{};
    bool found = false;
    for (int z = -4000; z <= 4000 && !found; z += 149)
        for (int x = -4000; x <= 4000 && !found; x += 137) {
            auto position = metres({double(x), -1, double(z)});
            double cx = double(position.anchor.x) * ChunkSpan + 4.125;
            double cz = double(position.anchor.z) * ChunkSpan + 4.125;
            double h = terrainHeight(Seed, cx, cz);
            if (h > -7 && h < -1) { key = position.anchor; found = true; }
        }
    require(found, "Coastline migration fixture not found");
    auto generated = generateChunk(Seed, key);
    auto solid = std::find_if(generated.begin(), generated.end(), [](auto v) { return opaque(v); });
    auto water = std::find(generated.begin(), generated.end(), Water);
    require(solid != generated.end() && water != generated.end(), "Migration fixture lacks land or water");
    auto carved = std::size_t(solid - generated.begin()), built = std::size_t(water - generated.begin());
    auto legacy = generated;
    std::replace(legacy.begin(), legacy.end(), std::uint8_t(Water), std::uint8_t(Air));
    legacy[carved] = Air;
    legacy[built] = 0x65;
    auto expected = generated;
    expected[carved] = Air;
    expected[built] = 0x65;
    auto expectedUpgrade = [&](ChunkKey atKey, ChunkData const& old) {
        auto result = generateChunk(Seed, atKey);
        for (std::size_t i = 0; i < result.size(); ++i)
            if (old[i] != Air || result[i] != Water) result[i] = old[i];
        return result;
    };
    for (int encoding = 0; encoding < 3; ++encoding) {
        auto directory = root / ("water-codec-" + std::to_string(encoding));
        legacyWorld(directory);
        auto old = legacy;
        if (encoding == 0) old.fill(Air);
        auto converted = expectedUpgrade(key, old);
        write(snapshot(directory, key), record(key, old, encoding, 4));
        {
            Store store(directory);
            require(store.load(key) == converted, "Legacy codec migration changed edits or lost ocean water");
            require(get32(read(directory / "manifest.bin"), 12) == 5, "Migration did not finish manifest");
        }
        { Store store(directory); require(store.load(key) == converted, "Migrated snapshot changed on reopen"); }
    }

    auto directory = root / "water-interrupted";
    legacyWorld(directory);
    ChunkKey next = *offsetKey(key, {1, 0, 0});
    ChunkKey already = *offsetKey(key, {2, 0, 0});
    ChunkData empty{};
    write(snapshot(directory, key), record(key, legacy, 2, 4));
    write(snapshot(directory, next), record(next, empty, 0, 4));
    write(snapshot(directory, already), record(already, empty, 0)); // Intentional v5 water carve.
    auto preservedV5 = read(snapshot(directory, already));
    auto blocker = snapshot(directory, next);
    blocker += ".tmp";
    fs::create_directory(blocker);
    fails([&] { Store store(directory); }, "WorldUpgradeBlocked");
    require(get32(read(directory / "manifest.bin"), 12) == 4, "Partial migration published v5 manifest");
    require(get32(read(snapshot(directory, key)), 12) == 5 &&
            get32(read(snapshot(directory, next)), 12) == 4, "Fixture did not interrupt between replacements");
    auto firstReplacement = read(snapshot(directory, key));
    fs::remove(blocker);
    fs::create_directory(directory / "manifest.bin.tmp");
    fails([&] { Store store(directory); }, "WorldUpgradeBlocked");
    require(get32(read(directory / "manifest.bin"), 12) == 4 &&
            get32(read(snapshot(directory, next)), 12) == 5, "Manifest was not the last migration write");
    require(read(snapshot(directory, key)) == firstReplacement &&
            read(snapshot(directory, already)) == preservedV5, "Resume reconverted an existing v5 snapshot");
    fs::remove(directory / "manifest.bin.tmp");
    {
        Store store(directory);
        require(store.load(key) == expected, "Interrupted conversion lost carved Air or opaque edit");
        require(store.load(next) == expectedUpgrade(next, empty), "Resume did not upgrade remaining v4 snapshot");
        require(store.load(already) == empty, "Resume refilled v5 carved water");
    }
    { Store store(directory); require(store.load(key) == expected, "Completed migration was not reopenable"); }

    directory = root / "water-recovery";
    legacyWorld(directory);
    ChunkData stale;
    stale.fill(Stone);
    auto staleBytes = record(key, stale, 0, 4);
    write(snapshot(directory, key), staleBytes);
    auto pending = journal({record(key, legacy, 2, 4)}, 4);
    auto corrupt = pending;
    corrupt.back() ^= 1;
    write(directory / "pending.txn", corrupt);
    fails([&] { Store store(directory); }, "checksum");
    require(read(snapshot(directory, key)) == staleBytes &&
            get32(read(directory / "manifest.bin"), 12) == 4 &&
            read(directory / "pending.txn") == corrupt, "Migration ran before validating v4 WAL");
    write(directory / "pending.txn", pending);
    blocker = snapshot(directory, key);
    blocker += ".tmp";
    fs::create_directory(blocker);
    fails([&] { Store store(directory); }, "DurableCheckpointBlocked");
    require(read(directory / "pending.txn") == pending &&
            read(snapshot(directory, key)) == staleBytes, "Failed v4 recovery discarded recoverable data");
    fs::remove(blocker);
    {
        Store store(directory);
        require(store.load(key) == expected, "Migration used stale snapshot instead of recovered v4 WAL");
        require(!fs::exists(directory / "pending.txn"), "Migration retained legacy WAL");
    }
    // Normal v5 opens must not continue accepting v4 journals or snapshots.
    write(directory / "pending.txn", pending);
    fails([&] { Store store(directory); }, "Unsupported journal version");
    fs::remove(directory / "pending.txn");
    write(snapshot(directory, key), record(key, legacy, 2, 4));
    fails([&] { Store store(directory); store.load(key); }, "Unsupported snapshot format/generator");

    directory = root / "water-corrupt-snapshot";
    legacyWorld(directory);
    auto good = record(key, legacy, 2, 4);
    corrupt = good;
    corrupt.back() ^= 1;
    write(snapshot(directory, key), corrupt);
    fails([&] { Store store(directory); }, "WorldUpgradeBlocked");
    require(read(snapshot(directory, key)) == corrupt &&
            get32(read(directory / "manifest.bin"), 12) == 4, "Migration ignored or destroyed a corrupt snapshot");
    write(snapshot(directory, key), good);
    { Store store(directory); require(store.load(key) == expected, "Repair could not resume migration"); }
    std::cout << "PASS v4 water conversion, edits, all codecs, interrupted replacements/manifest, WAL-first recovery, strict v5 reopen\n";
}

void negativeBrushAndAir(fs::path const& root) {
    auto directory = root / "brush";
    Brush brush{metres({-8.0, 0.0, -8.0}), .25f, 0x55};
    std::vector<Chunk> edited;
    {
        Store store(directory, Seed);
        fails([&] { store.load({0, 0, 0, 1}); }, "Store only holds level-0 chunks");
        fails([&] { store.edit({at({0, 0, 0, 1}, glm::dvec3(4.0)), .25f, Stone}); }, "Store only holds level-0 chunks");
        require(store.savedCount() == 0 && !fs::exists(directory / "pending.txn"), "Level check mutated the store");
    }
    {
        Store store(directory);
        edited = store.edit(brush);
        require(edited.size() == 8, "Negative corner brush did not edit eight chunks");
        for (auto const& chunk : edited) {
            require(chunk.key.x == -2 || chunk.key.x == -1, "Negative X floor ownership wrong");
            require(chunk.key.y == -1 || chunk.key.y == 0, "Y floor ownership wrong");
            require(chunk.key.z == -2 || chunk.key.z == -1, "Negative Z floor ownership wrong");
            require(chunk.key.level == 0 && chunk.data != nullptr, "Edited chunk is not a level-0 data chunk");
            auto before = generateChunk(Seed, chunk.key);
            auto expected = before;
            int x = chunk.key.x == -2 ? 31 : 0;
            int y = chunk.key.y == -1 ? 31 : 0;
            int z = chunk.key.z == -2 ? 31 : 0;
            expected[index(x, y, z)] = brush.material;
            require(*chunk.data == expected, "Brush changed voxels outside center-sampled sphere");
            require(store.load(chunk.key) == *chunk.data, "Acknowledged edit was not readable");
        }
        require(!fs::exists(directory / "pending.txn"), "Successful edit retained pending journal");
        require(store.savedCount() == 8, "Committed keys missing from saved set");
    }
    { Store store(directory); for (auto const& chunk : edited)
        require(store.load(chunk.key) == *chunk.data, "Restart lost negative cross-chunk edit"); }

    auto key = ground(0, 0);
    ChunkData empty{};
    auto generated = generateChunk(Seed, key);
    require(std::any_of(generated.begin(), generated.end(), [](auto v) { return v != Air; }),
            "All-air persistence fixture must replace generated solid terrain");
    write(snapshot(directory, key), record(key, empty, 0));
    {
        Store store(directory);
        require(store.load(key) == empty, "Saved air regenerated");
        Brush one{at(key, glm::dvec3(1.125)), .25f, Crystal};
        auto added = store.edit(one);
        require(added.size() == 1 && (*added[0].data)[index(4, 4, 4)] == Crystal, "Interior brush insertion failed");
        one.material = Air;
        auto erased = store.edit(one);
        require(erased.size() == 1 && *erased[0].data == empty, "Last solid brush did not erase to air");
    }
    { Store store(directory); require(store.load(key) == empty, "All-air edit lost on restart"); }
    auto saved = read(snapshot(directory, key));
    require(get32(saved, 52) == 0 && saved.size() == Header + 1 + 4, "All-air snapshot is not explicit uniform encoding");
    std::cout << "PASS negative eight-chunk center sampling, tint, durable restart, edited all-air snapshot, level-0 only\n";
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
            auto changes = store.edit({at(key, glm::dvec3(1.125)), .25f, Crystal});
            require(changes.size() == 1, "Codec edit did not change fixture");
            auto expected = *values[code];
            for (int z = 3; z <= 5; ++z) for (int y = 3; y <= 5; ++y) for (int x = 3; x <= 5; ++x)
                if ((x - 4) * (x - 4) + (y - 4) * (y - 4) + (z - 4) * (z - 4) <= 1)
                    expected[index(x, y, z)] = Crystal;
            require(*changes[0].data == expected && store.load(key) == expected, "Encoded edit lost data");
            require(get32(read(snapshot(directory, key)), 52) == std::uint32_t(code), "Encoder chose nonminimal codec");
        }
    }
    {
        // Fixtures mutate files only with Store closed; corrupt disk reads must never regenerate.
        auto load = [&](ChunkKey key) { Store store(directory); return store.load(key); };
        ChunkKey key{10, 0, 0};
        auto original = read(snapshot(directory, key));
        auto broken = original;
        broken[Header] ^= 1;
        write(snapshot(directory, key), broken);
        fails([&] { load(key); }, "checksum");
        broken = original;
        set32(broken, 56, 0xffffffffu);
        reseal(broken);
        write(snapshot(directory, key), broken);
        fails([&] { load(key); }, "payload length");
        broken = original;
        broken[Header] = 0x10; // A tinted air byte is invalid, not empty.
        reseal(broken);
        write(snapshot(directory, key), broken);
        fails([&] { load(key); }, "material");
        broken = original;
        set32(broken, 24, 999);
        reseal(broken);
        write(snapshot(directory, key), broken);
        fails([&] { load(key); }, "filename/key");
        broken = original;
        set32(broken, 48, 1);
        reseal(broken);
        write(snapshot(directory, key), broken);
        fails([&] { load(key); }, "level");
        broken = original;
        set32(broken, 8, 1);
        reseal(broken);
        write(snapshot(directory, key), broken);
        fails([&] { load(key); }, "Unsupported snapshot format");
        write(snapshot(directory, key), original);
        fs::resize_file(snapshot(directory, key), 1024 * 1024 * 1024);
        fails([&] { load(key); }, "length");
        write(snapshot(directory, key), original);
        fs::resize_file(snapshot(directory, key), 12);
        fails([&] { load(key); }, "length");
        write(snapshot(directory, key), original);
        auto malformedRle = record({11, 0, 0}, rle, 1);
        malformedRle[Header] = malformedRle[Header + 1] = 0;
        reseal(malformedRle);
        write(snapshot(directory, {11, 0, 0}), malformedRle);
        fails([&] { load({11, 0, 0}); }, "RLE run");
        fs::remove(snapshot(directory, key));
        fs::create_directory(snapshot(directory, key));
        fails([&] { load(key); }, "type/length");
    }
    std::cout << "PASS uniform/RLE/raw codec, encoder selection, corruption, checksum/identity/level/material/length bounds\n";
}

void failureAndRecovery(fs::path const& root) {
    auto directory = root / "failure";
    initialize(directory);
    Brush brush{metres({-8.0, 0.0, -8.0}), .25f, 0x55};
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
        require(store.savedCount() == keys.size(), "Journal recovery did not populate saved keys");
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

void farKeys(fs::path const& root) {
    auto directory = root / "far";
    std::array<ChunkKey, 4> corners{ChunkKey{Big, Big, Big}, ChunkKey{-Big, -Big, -Big},
                                    ChunkKey{Big, -Big, Big}, ChunkKey{-Big, Big, -Big}};
    std::vector<std::pair<ChunkKey, ChunkData>> committed;
    {
        Store store(directory, Seed);
        for (auto corner : corners) {
            auto edited = store.edit({at(corner, glm::dvec3(0.0)), .25f, Crystal});
            require(edited.size() == 8, "Corner brush at +-2^62 did not span eight chunks");
            for (auto& chunk : edited) {
                require(chunk.key.level == 0 && std::abs(chunk.key.x - corner.x) <= 1 &&
                        std::abs(chunk.key.y - corner.y) <= 1 && std::abs(chunk.key.z - corner.z) <= 1,
                        "Far brush touched a non-neighbouring chunk");
                committed.emplace_back(chunk.key, *chunk.data);
            }
        }
        for (auto const& [key, data] : committed) require(store.load(key) == data, "Far edit not readable");
        ChunkKey neighbours[2]{{Fine, 0, Fine}, {Fine + 1, 0, Fine}};
        ChunkData a, b;
        a.fill(Stone);
        b.fill(Sand);
        write(snapshot(directory, neighbours[0]), record(neighbours[0], a, 0));
        write(snapshot(directory, neighbours[1]), record(neighbours[1], b, 0));
    }
    {
        Store store(directory);
        require(store.savedCount() == committed.size() + 2, "Directory scan missed far snapshots");
        for (auto const& [key, data] : committed) require(store.load(key) == data, "Far edit lost on reopen");
        ChunkData a, b;
        a.fill(Stone);
        b.fill(Sand);
        require(store.load({Fine, 0, Fine}) == a && store.load({Fine + 1, 0, Fine}) == b,
                "Adjacent keys beyond 2^53 collapsed");
        auto edited = store.edit({at({Fine, 0, Fine}, glm::dvec3(7.9, 3.875, 3.875)), .25f, Crystal});
        require(edited.size() == 2 && edited[0].key == ChunkKey{Fine, 0, Fine} && edited[1].key == ChunkKey{Fine + 1, 0, Fine},
                "Brush across 2^53 boundary did not edit both neighbours");
        require((*edited[1].data)[index(0, 15, 15)] == Crystal && (*edited[1].data)[index(1, 15, 15)] == Sand,
                "Brush across 2^53 boundary mis-sampled the neighbour");

        constexpr auto max = std::numeric_limits<std::int64_t>::max();
        auto before = readAll(directory);
        auto savedBefore = store.savedCount();
        fails([&] { store.edit({at({max, Big, 0}, glm::dvec3(7.9, 4.0, 4.0)), .25f, Crystal}); }, "not representable");
        require(readAll(directory) == before && store.savedCount() == savedBefore,
                "Unrepresentable brush footprint mutated the store");
    }
    {
        Store store(directory);
        constexpr auto min = std::numeric_limits<std::int64_t>::min();
        fails([&] { store.edit({at({0, min, 0}, glm::dvec3(4.0, 0.1, 4.0)), .25f, Crystal}); }, "not representable");
    }
    {
        Store store(directory);
        constexpr auto max = std::numeric_limits<std::int64_t>::max();
        auto interior = store.edit({at({max, Big, 0}, glm::dvec3(4.0)), .25f, Crystal});
        require(interior.size() == 1 && interior[0].key == ChunkKey{max, Big, 0}, "Interior brush at INT64_MAX rejected");
    }
    std::cout << "PASS int64 keys at +-2^62, adjacent keys beyond 2^53 distinct, unrepresentable footprints rejected untouched\n";
}

void savedWithin(fs::path const& root) {
    auto directory = root / "within";
    ChunkData solid;
    solid.fill(Stone);
    std::vector<ChunkKey> saved{{0, 0, 0}, {7, 7, 7}, {8, 0, 0}, {-1, -1, -1}, {-8, 0, 0}, {3, 9, 3}, {Big, Big, Big}, {Big + 7, Big, Big + 1}};
    initialize(directory);
    for (auto key : saved) write(snapshot(directory, key), record(key, solid, 0));
    write(directory / "chunk_1_2.bin", Bytes{1});
    write(directory / "chunk_+1_2_3.bin", Bytes{1});
    write(directory / "chunk_01_2_3.bin", Bytes{1});
    Store store(directory);
    require(store.savedCount() == saved.size(), "Non-canonical snapshot names were counted");
    auto expect = [&](ChunkKey coarse, std::vector<ChunkKey> expected) {
        std::sort(expected.begin(), expected.end());
        require(store.savedKeysWithin(coarse) == expected, "savedKeysWithin mismatch at level " + std::to_string(coarse.level));
    };
    expect({0, 0, 0, 3}, {{0, 0, 0}, {7, 7, 7}});
    expect({0, 0, 0, 2}, {{0, 0, 0}});
    expect({1, 1, 1, 2}, {{7, 7, 7}});
    expect({0, 0, 0, 1}, {{0, 0, 0}});
    expect({1, 0, 0, 3}, {{8, 0, 0}});
    expect({-1, -1, -1, 3}, {{-1, -1, -1}});
    expect({-1, 0, 0, 3}, {{-8, 0, 0}});
    expect({-1, 0, 0, 1}, {});
    expect({-1, -1, -1, 1}, {{-1, -1, -1}});
    expect({0, 1, 0, 3}, {{3, 9, 3}});
    expect({0, 0, 0, 4}, {{0, 0, 0}, {7, 7, 7}, {8, 0, 0}, {3, 9, 3}});
    expect({0, 0, 0, 0}, {{0, 0, 0}});
    expect({1, 0, 0, 0}, {});
    expect({Big >> 3, Big >> 3, Big >> 3, 3}, {{Big, Big, Big}, {Big + 7, Big, Big + 1}});
    expect({Big >> 1, Big >> 1, Big >> 1, 1}, {{Big, Big, Big}});
    require(store.savedKeysWithin({std::numeric_limits<std::int64_t>::max(), 0, 0, 1}).empty(),
            "Unrepresentable coarse corner returned keys");
    auto edited = store.edit({at({16, 0, 16}, glm::dvec3(4.0)), .25f, Crystal});
    require(edited.size() == 1, "Within edit did not commit");
    expect({2, 0, 2, 3}, {{16, 0, 16}});
    std::cout << "PASS savedKeysWithin at levels 0..4 and +-2^62, canonical filename scan, committed keys visible\n";
}

void cacheTravel(fs::path const& root) {
    auto directory = root / "travel";
    {
        Store store(directory, Seed);
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i != 1000; ++i) {
            ChunkKey key{i - 500, (i % 8) - 2, 137};
            auto data = store.load(key);
            require(data == generateChunk(Seed, key), "Travel generation differs from pure generator");
            checkBounds(store);
        }
        auto milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::cout << "PASS 1000 generated-chunk travel: " << milliseconds << " ms, "
                  << store.cachedBytes() << " allocated encoded bytes, " << store.cachedChunks() << " entries\n";
        ChunkData air{}, bedrock;
        bedrock.fill(Bedrock);
        require(store.load({0, 100, 0}) == air && store.load({0, -100, 0}) == bedrock,
                "Trivial uniform columns differ from generator contract");
    }
    ChunkData raw;
    for (std::size_t i = 0; i < raw.size(); ++i) raw[i] = static_cast<std::uint8_t>(1 + i % 5);
    // Exercise BOTH caps at the actual limit, not just a small-map upper-bound assertion.
    for (int i = 0; i != 2050; ++i) {
        ChunkKey key{i, 3, 500};
        write(snapshot(directory, key), record(key, raw, 2));
    }
    Store reopened(directory);
    for (int i = 0; i != 2050; ++i) {
        require(reopened.load({i, 3, 500}) == raw, "Raw travel cache changed snapshot");
        checkBounds(reopened);
    }
    require(reopened.cachedChunks() == 2048 && reopened.cachedBytes() == 64 * 1024 * 1024,
            "Worst-case raw cache did not hit and hold both limits");
    require(reopened.load({0, 3, 500}) == raw, "Evicted saved snapshot did not reload");
    checkBounds(reopened);
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
            auto candidate = base / ("mountain-world-storage-" + std::to_string(stamp) + "-" + std::to_string(attempt));
            if (fs::create_directory(candidate)) { root = candidate; break; }
        }
        require(!root.empty(), "Could not create unique smoke directory");
        manifestAndLock(root);
        waterMigration(root);
        negativeBrushAndAir(root);
        codecsAndCorruption(root);
        failureAndRecovery(root);
        farKeys(root);
        savedWithin(root);
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
