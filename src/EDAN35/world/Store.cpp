#include "Store.hpp"
#include "Generate.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <list>
#include <map>
#include <set>
#include <span>
#include <string>
#include <system_error>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace world {
#ifndef _WIN32
namespace {
namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;
constexpr std::uint32_t FormatVersion = 2;
constexpr std::size_t RawSize = ChunkSize * ChunkSize * ChunkSize;
constexpr std::size_t RecordHeader = 60;
constexpr std::size_t MaxRecord = RecordHeader + RawSize + 4;
constexpr std::size_t JournalHeader = 32;
constexpr std::size_t MaxJournal = JournalHeader + MaxBrushChunks * (4 + MaxRecord) + 4;
constexpr std::size_t ManifestSize = 44;
constexpr std::size_t CacheBytes = 64 * 1024 * 1024;
constexpr std::size_t CacheEntries = 2048;
constexpr char ManifestMagic[] = "FWORLD01";
constexpr char RecordMagic[] = "FWCHNK01";
constexpr char JournalMagic[] = "FWTXN001";

enum class Encoding : std::uint32_t { Uniform, Rle, Raw };

[[noreturn]] void fail(std::string const& message) { throw std::runtime_error(message); }
[[noreturn]] void ioError(char const* operation, fs::path const& path) {
    int error = errno;
    fail(std::string(operation) + " " + path.string() + ": " +
         std::error_code(error, std::generic_category()).message());
}

struct File {
    int fd = -1;
    explicit File(int descriptor = -1) : fd(descriptor) {}
    ~File() { if (fd >= 0) ::close(fd); }
    File(File const&) = delete;
    File& operator=(File const&) = delete;
    void close(fs::path const& path) {
        int descriptor = fd;
        fd = -1; // close must not be retried: the descriptor may already be released.
        if (::close(descriptor) != 0) ioError("close", path);
    }
};

void syncFile(int fd, fs::path const& path) {
    int result;
#ifdef __APPLE__
    do { result = ::fcntl(fd, F_FULLFSYNC); } while (result < 0 && errno == EINTR);
#else
    do { result = ::fsync(fd); } while (result < 0 && errno == EINTR);
#endif
    if (result < 0) ioError("file durability barrier", path);
}

void syncDirectory(fs::path const& path) {
    File file(::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (file.fd < 0) ioError("open directory", path);
    int result;
    do { result = ::fsync(file.fd); } while (result < 0 && errno == EINTR);
    if (result < 0) ioError("directory durability barrier", path);
    file.close(path);
}

void ensureDirectory(fs::path const& path) {
    std::error_code error;
    auto status = fs::status(path, error);
    if (!error && fs::is_directory(status)) return;
    if (!error && fs::exists(status)) fail("Not a world directory: " + path.string());
    if (error && error != std::errc::no_such_file_or_directory)
        fail("stat directory " + path.string() + ": " + error.message());
    auto parent = path.parent_path();
    if (parent != path && !parent.empty()) ensureDirectory(parent);
    if (!fs::create_directory(path, error) && error)
        fail("create directory " + path.string() + ": " + error.message());
    syncDirectory(path);
    if (!parent.empty()) syncDirectory(parent);
}

std::optional<Bytes> readFile(fs::path const& path, std::size_t maximum) {
    File file(::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK));
    if (file.fd < 0) {
        if (errno == ENOENT) return std::nullopt;
        ioError("open", path);
    }
    struct stat status{};
    if (::fstat(file.fd, &status) != 0) ioError("stat", path);
    if (!S_ISREG(status.st_mode) || status.st_size < 0 ||
        static_cast<std::uint64_t>(status.st_size) > maximum)
        fail("Invalid file type/length: " + path.string());
    Bytes data(static_cast<std::size_t>(status.st_size));
    std::size_t done = 0;
    while (done < data.size()) {
        auto count = ::read(file.fd, data.data() + done, data.size() - done);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) ioError("read", path);
        if (count == 0) fail("Truncated file: " + path.string());
        done += static_cast<std::size_t>(count);
    }
    std::uint8_t extra;
    ssize_t count;
    do { count = ::read(file.fd, &extra, 1); } while (count < 0 && errno == EINTR);
    if (count < 0) ioError("read trailing byte", path);
    if (count != 0) fail("File changed length while reading: " + path.string());
    file.close(path);
    return data;
}

void writeFile(fs::path const& path, std::span<const std::uint8_t> data) {
    File file(::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600));
    if (file.fd < 0) ioError("open temporary file", path);
    std::size_t done = 0;
    while (done < data.size()) {
        auto count = ::write(file.fd, data.data() + done, data.size() - done);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) ioError("write", path);
        if (count == 0) fail("Zero-length write: " + path.string());
        done += static_cast<std::size_t>(count);
    }
    syncFile(file.fd, path);
    file.close(path);
}

void renameFile(fs::path const& from, fs::path const& to) {
    if (::rename(from.c_str(), to.c_str()) != 0) ioError("rename to", to);
}
void replaceFile(fs::path const& target, std::span<const std::uint8_t> data) {
    auto temporary = target;
    temporary += ".tmp";
    writeFile(temporary, data);
    renameFile(temporary, target);
    syncDirectory(target.parent_path());
}

// CRC-32/ISO-HDLC, covering identity-bearing headers as well as payloads.
std::uint32_t crc(std::span<const std::uint8_t> bytes) {
    static constexpr auto table = [] {
        std::array<std::uint32_t, 256> values{};
        for (std::uint32_t i = 0; i < values.size(); ++i) {
            auto value = i;
            for (int bit = 0; bit < 8; ++bit)
                value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0);
            values[i] = value;
        }
        return values;
    }();
    std::uint32_t value = 0xffffffffu;
    for (auto byte : bytes) value = table[(value ^ byte) & 255] ^ (value >> 8);
    return value ^ 0xffffffffu;
}
void put32(Bytes& data, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) data.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}
void put64(Bytes& data, std::uint64_t value) {
    put32(data, static_cast<std::uint32_t>(value));
    put32(data, static_cast<std::uint32_t>(value >> 32));
}
void magic(Bytes& data, char const* value) { data.insert(data.end(), value, value + 8); }
void seal(Bytes& data, std::size_t start = 0) {
    put32(data, crc(std::span<const std::uint8_t>(data).subspan(start)));
}
struct Reader {
    std::span<const std::uint8_t> data;
    std::size_t offset = 0;
    std::span<const std::uint8_t> take(std::size_t count) {
        if (count > data.size() - offset) fail("Truncated world record");
        auto result = data.subspan(offset, count);
        offset += count;
        return result;
    }
    std::uint32_t u32() {
        auto bytes = take(4);
        return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) |
               (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
    }
    std::uint64_t u64() { auto low = u32(); return low | (std::uint64_t(u32()) << 32); }
    void expectMagic(char const* expected) {
        auto bytes = take(8);
        if (!std::equal(bytes.begin(), bytes.end(), expected)) fail("Invalid world record magic");
    }
    void end() { if (offset != data.size()) fail("Trailing world record bytes"); }
};
std::span<const std::uint8_t> checked(std::span<const std::uint8_t> data) {
    if (data.size() < 4) fail("Truncated world checksum");
    Reader checksum{data.last(4)};
    auto body = data.first(data.size() - 4);
    if (crc(body) != checksum.u32()) fail("World record checksum mismatch");
    return body;
}
bool materialValid(std::uint8_t byte) {
    return byte == Air || ((byte & MaterialMask) >= Grass && (byte & MaterialMask) <= Bedrock);
}
void validateMaterials(std::span<const std::uint8_t> data) {
    for (auto byte : data) if (!materialValid(byte)) fail("Invalid world material byte");
}

struct EncodingInfo { Encoding kind; std::size_t bytes; };
EncodingInfo chooseEncoding(ChunkData const& data) {
    std::size_t runs = 1;
    for (std::size_t i = 1; i < data.size(); ++i) runs += data[i] != data[i - 1];
    if (runs == 1) return {Encoding::Uniform, 1};
    if (runs * 3 < data.size()) return {Encoding::Rle, runs * 3};
    return {Encoding::Raw, data.size()};
}
void encodeInto(Bytes& result, ChunkData const& data, Encoding kind) {
    if (kind == Encoding::Uniform) { result.push_back(data[0]); return; }
    if (kind == Encoding::Raw) { result.insert(result.end(), data.begin(), data.end()); return; }
    for (std::size_t begin = 0; begin < data.size();) {
        auto end = begin + 1;
        while (end < data.size() && data[end] == data[begin]) ++end;
        auto count = end - begin;
        result.push_back(static_cast<std::uint8_t>(count));
        result.push_back(static_cast<std::uint8_t>(count >> 8));
        result.push_back(data[begin]);
        begin = end;
    }
}
ChunkData decode(Encoding kind, std::span<const std::uint8_t> payload) {
    ChunkData data;
    if (kind == Encoding::Uniform) {
        if (payload.size() != 1) fail("Invalid uniform payload length");
        validateMaterials(payload);
        data.fill(payload[0]);
    } else if (kind == Encoding::Raw) {
        if (payload.size() != RawSize) fail("Invalid raw payload length");
        validateMaterials(payload);
        std::copy(payload.begin(), payload.end(), data.begin());
    } else if (kind == Encoding::Rle) {
        if (payload.empty() || payload.size() >= RawSize || payload.size() % 3 != 0)
            fail("Invalid RLE payload length");
        std::size_t written = 0;
        for (std::size_t i = 0; i < payload.size(); i += 3) {
            std::size_t count = payload[i] | (std::size_t(payload[i + 1]) << 8);
            if (count == 0 || count > RawSize - written || !materialValid(payload[i + 2]))
                fail("Invalid RLE run");
            std::fill_n(data.begin() + written, count, payload[i + 2]);
            written += count;
        }
        if (written != RawSize) fail("Incomplete RLE output");
    } else fail("Unsupported chunk encoding");
    return data;
}

struct Record { ChunkKey key; ChunkData data; };
void appendRecord(Bytes& result, std::uint64_t seed, ChunkKey key, ChunkData const& data) {
    auto encoding = chooseEncoding(data);
    auto begin = result.size();
    magic(result, RecordMagic);
    put32(result, FormatVersion);
    put32(result, GeneratorVersion);
    put64(result, seed);
    put64(result, static_cast<std::uint64_t>(key.x));
    put64(result, static_cast<std::uint64_t>(key.y));
    put64(result, static_cast<std::uint64_t>(key.z));
    put32(result, key.level);
    put32(result, static_cast<std::uint32_t>(encoding.kind));
    put32(result, static_cast<std::uint32_t>(encoding.bytes));
    encodeInto(result, data, encoding.kind);
    seal(result, begin);
}
Record parseRecord(std::span<const std::uint8_t> bytes, std::uint64_t seed) {
    if (bytes.size() < RecordHeader + 5 || bytes.size() > MaxRecord) fail("Invalid snapshot length");
    Reader reader{checked(bytes)};
    reader.expectMagic(RecordMagic);
    if (reader.u32() != FormatVersion || reader.u32() != GeneratorVersion)
        fail("Unsupported snapshot format/generator version");
    if (reader.u64() != seed) fail("Snapshot seed mismatch");
    ChunkKey key{static_cast<std::int64_t>(reader.u64()), static_cast<std::int64_t>(reader.u64()),
                 static_cast<std::int64_t>(reader.u64())};
    if (reader.u32() != 0) fail("Snapshot level is not zero");
    auto encoding = static_cast<Encoding>(reader.u32());
    auto length = reader.u32();
    if (length > RawSize) fail("Invalid snapshot payload length");
    auto payload = reader.take(length);
    reader.end();
    return {key, decode(encoding, payload)};
}
fs::path chunkPath(fs::path const& directory, ChunkKey key) {
    return directory / ("chunk_" + std::to_string(key.x) + "_" + std::to_string(key.y) + "_" +
                        std::to_string(key.z) + ".bin");
}
// Only exactly canonical "chunk_<x>_<y>_<z>.bin" names are snapshots; anything else is ignored.
std::optional<ChunkKey> parseChunkName(std::string const& name) {
    if (!name.starts_with("chunk_") || !name.ends_with(".bin")) return std::nullopt;
    std::int64_t values[3];
    std::size_t begin = 6, end = name.size() - 4;
    for (int axis = 0; axis < 3; ++axis) {
        auto stop = axis == 2 ? end : name.find('_', begin);
        if (stop == std::string::npos || stop > end || stop == begin) return std::nullopt;
        auto token = name.substr(begin, stop - begin);
        auto parsed = std::from_chars(token.data(), token.data() + token.size(), values[axis]);
        if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size() ||
            std::to_string(values[axis]) != token)
            return std::nullopt;
        begin = stop + 1;
    }
    return ChunkKey{values[0], values[1], values[2], 0};
}
int floorChunk(int voxel) { return voxel >= 0 ? voxel / ChunkSize : (voxel + 1) / ChunkSize - 1; }
void requireLevelZero(ChunkKey key) { if (key.level != 0) fail("Store only holds level-0 chunks"); }
}

struct Store::Impl {
    fs::path directory;
    File lock;
    std::uint64_t worldSeed = 0;
    bool blocked = false;
    struct Cached {
        Encoding encoding;
        Bytes payload;
        std::list<ChunkKey>::iterator age;
    };
    std::map<ChunkKey, Cached> cache;
    std::list<ChunkKey> ages;
    std::size_t cacheBytes = 0;
    std::set<ChunkKey> savedKeys;

    Impl(fs::path path, std::optional<std::uint64_t> requested) : directory(fs::absolute(path)) {
        ensureDirectory(directory);
        auto lockPath = directory / "world.lock";
        lock.fd = ::open(lockPath.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (lock.fd < 0) ioError("open world lock", lockPath);
        if (::flock(lock.fd, LOCK_EX | LOCK_NB) != 0) ioError("WorldInUse: exclusive lock", lockPath);
        auto manifest = readFile(directory / "manifest.bin", ManifestSize);
        if (manifest) {
            if (manifest->size() != ManifestSize) fail("Invalid manifest length");
            Reader reader{checked(*manifest)};
            reader.expectMagic(ManifestMagic);
            if (reader.u32() != FormatVersion || reader.u32() != GeneratorVersion ||
                reader.u32() != ChunkSize || reader.u32() != 2 ||
                reader.u32() != 1 || reader.u32() != 4)
                fail("Unsupported world format/generator/material/voxel schema");
            worldSeed = reader.u64();
            reader.end();
            if (requested && *requested != worldSeed) fail("Requested seed differs from world manifest");
        } else {
            for (auto const& entry : fs::directory_iterator(directory))
                if (entry.path().filename() != "world.lock")
                    fail("Missing manifest in nonempty world directory: " + directory.string());
            worldSeed = requested.value_or(DefaultSeed);
            Bytes bytes;
            bytes.reserve(ManifestSize);
            magic(bytes, ManifestMagic);
            put32(bytes, FormatVersion);
            put32(bytes, GeneratorVersion);
            put32(bytes, ChunkSize);
            put32(bytes, 2); // Material schema.
            put32(bytes, 1); // Voxel scale numerator / denominator.
            put32(bytes, 4);
            put64(bytes, worldSeed);
            seal(bytes);
            replaceFile(directory / "manifest.bin", bytes);
        }
        for (auto const& entry : fs::directory_iterator(directory))
            if (auto key = parseChunkName(entry.path().filename().string())) savedKeys.insert(*key);
        try {
            auto pending = readFile(directory / "pending.txn", MaxJournal);
            if (pending) {
                auto records = validateJournal(*pending);
                checkpoint(*pending, records);
            }
        } catch (std::exception const& error) {
            fail(std::string("DurableCheckpointBlocked: recovery: ") + error.what());
        }
    }

    void eraseCache(ChunkKey key) {
        auto found = cache.find(key);
        if (found == cache.end()) return;
        cacheBytes -= found->second.payload.capacity();
        ages.erase(found->second.age);
        cache.erase(found);
    }
    void cacheData(ChunkKey key, ChunkData const& data) {
        eraseCache(key);
        auto info = chooseEncoding(data);
        Bytes payload;
        payload.reserve(info.bytes);
        encodeInto(payload, data, info.kind);
        while (!ages.empty() && (cache.size() >= CacheEntries ||
               cacheBytes + payload.capacity() > CacheBytes)) eraseCache(ages.back());
        if (payload.capacity() > CacheBytes) return;
        ages.push_front(key);
        try {
            auto [position, inserted] = cache.emplace(key, Cached{info.kind, std::move(payload), ages.begin()});
            (void)inserted;
            cacheBytes += position->second.payload.capacity();
        } catch (...) {
            ages.pop_front();
            throw;
        }
    }
    ChunkData load(ChunkKey key) {
        if (blocked) fail("StorageBlocked: reconstruct Store to recover before further requests");
        requireLevelZero(key);
        // The exclusive writer lock and commit invalidation keep cached snapshots current.
        auto found = cache.find(key);
        if (found != cache.end()) {
            ages.splice(ages.begin(), ages, found->second.age);
            return decode(found->second.encoding, found->second.payload);
        }
        if (savedKeys.contains(key)) {
            auto path = chunkPath(directory, key);
            auto saved = readFile(path, MaxRecord);
            if (!saved) fail(path.string() + ": saved snapshot vanished");
            try {
                auto record = parseRecord(*saved, worldSeed);
                if (record.key != key) fail("Snapshot filename/key mismatch");
                cacheData(key, record.data);
                return record.data;
            } catch (std::exception const& error) {
                fail(path.string() + ": " + error.what());
            }
        }
        ChunkData data;
        if (auto uniform = trivialUniform(worldSeed, key)) data.fill(*uniform);
        else data = generateChunk(worldSeed, key);
        validateMaterials(data);
        cacheData(key, data);
        return data;
    }

    std::vector<ChunkKey> savedKeysWithin(ChunkKey coarse) const {
        std::vector<ChunkKey> result;
        auto corner = levelZeroCorner(coarse);
        if (!corner) return result;
        // corner is a multiple of 2^level, so corner + 2^level - 1 never overflows.
        auto extent = (std::int64_t{1} << coarse.level) - 1;
        ChunkKey hi{corner->x + extent, corner->y + extent, corner->z + extent, 0};
        constexpr auto min = std::numeric_limits<std::int64_t>::min();
        for (auto it = savedKeys.lower_bound({corner->x, min, min, 0}); it != savedKeys.end() && it->x <= hi.x; ++it)
            if (it->y >= corner->y && it->y <= hi.y && it->z >= corner->z && it->z <= hi.z) result.push_back(*it);
        return result;
    }

    struct RecordView { ChunkKey key; std::size_t offset, size; };
    std::vector<RecordView> validateJournal(std::span<const std::uint8_t> bytes) {
        if (bytes.size() < JournalHeader + 4 || bytes.size() > MaxJournal) fail("Invalid journal length");
        Reader reader{checked(bytes)};
        reader.expectMagic(JournalMagic);
        if (reader.u32() != FormatVersion || reader.u32() != GeneratorVersion)
            fail("Unsupported journal version/generator");
        if (reader.u64() != worldSeed) fail("Journal seed mismatch");
        auto count = reader.u32();
        if (count < 1 || count > MaxBrushChunks) fail("Invalid journal chunk count");
        if (reader.u32() != reader.data.size() - JournalHeader) fail("Invalid journal body length");
        std::vector<RecordView> records;
        records.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            auto length = reader.u32();
            if (length > MaxRecord) fail("Invalid journal snapshot length");
            auto offset = reader.offset;
            auto record = parseRecord(reader.take(length), worldSeed);
            for (auto const& previous : records)
                if (previous.key == record.key) fail("Duplicate journal chunk key");
            records.push_back({record.key, offset, length});
        }
        reader.end();
        return records;
    }
    void checkpoint(std::span<const std::uint8_t> journal, std::vector<RecordView> const& records) {
        for (auto const& record : records) {
            replaceFile(chunkPath(directory, record.key), journal.subspan(record.offset, record.size));
            savedKeys.insert(record.key);
        }
        auto pending = directory / "pending.txn";
        if (::unlink(pending.c_str()) != 0) ioError("remove committed journal", pending);
        syncDirectory(directory);
    }

    std::vector<Chunk> edit(Brush brush) {
        if (blocked) fail("StorageBlocked: reconstruct Store to recover before further requests");
        char const* phase = "FailedBeforeCommit";
        try {
            requireLevelZero(brush.center.anchor);
            auto center = normalizedPosition(brush.center.anchor, brush.center.offset);
            if (!std::isfinite(brush.radius) || brush.radius < .25f || brush.radius > 2.f || !center ||
                !materialValid(brush.material))
                fail("Invalid brush radius, center, or material");
            // Voxel coordinates relative to the anchor chunk's corner; the brush footprint stays within
            // the anchor's neighbours because the radius is at most a quarter chunk.
            std::array<int, 3> low{}, high{};
            glm::ivec3 firstDelta, lastDelta;
            for (int axis = 0; axis < 3; ++axis) {
                double voxel = center->offset[axis] / VoxelScale;
                double radius = double(brush.radius) / VoxelScale;
                low[axis] = static_cast<int>(std::ceil(voxel - radius - .5));
                high[axis] = static_cast<int>(std::floor(voxel + radius - .5));
                firstDelta[axis] = floorChunk(low[axis]);
                lastDelta[axis] = floorChunk(high[axis]);
            }
            auto first = offsetKey(center->anchor, firstDelta);
            auto last = offsetKey(center->anchor, lastDelta);
            if (!first || !last) fail("Brush chunk footprint is not representable");
            int count = int(last->x - first->x + 1) * int(last->y - first->y + 1) * int(last->z - first->z + 1);
            if (count < 1 || count > MaxBrushChunks) fail("Brush exceeds eight chunks");
            std::vector<Chunk> chunks;
            chunks.reserve(count);
            double radiusSquared = double(brush.radius) * brush.radius;
            for (int z = firstDelta.z; z <= lastDelta.z; ++z)
                for (int y = firstDelta.y; y <= lastDelta.y; ++y)
                    for (int x = firstDelta.x; x <= lastDelta.x; ++x) {
                        auto key = *offsetKey(center->anchor, {x, y, z});
                        auto data = std::make_unique<ChunkData>(load(key));
                        bool changed = false;
                        for (int vz = std::max(low[2], z * ChunkSize); vz <= std::min(high[2], (z + 1) * ChunkSize - 1); ++vz)
                            for (int vy = std::max(low[1], y * ChunkSize); vy <= std::min(high[1], (y + 1) * ChunkSize - 1); ++vy)
                                for (int vx = std::max(low[0], x * ChunkSize); vx <= std::min(high[0], (x + 1) * ChunkSize - 1); ++vx) {
                                    double dx = (vx + .5) * VoxelScale - center->offset.x;
                                    double dy = (vy + .5) * VoxelScale - center->offset.y;
                                    double dz = (vz + .5) * VoxelScale - center->offset.z;
                                    auto& voxel = (*data)[index(vx - x * ChunkSize, vy - y * ChunkSize, vz - z * ChunkSize)];
                                    if (dx * dx + dy * dy + dz * dz <= radiusSquared && voxel != brush.material) {
                                        voxel = brush.material;
                                        changed = true;
                                    }
                                }
                        if (changed) chunks.push_back({key, Air, std::move(data)});
                    }
            if (chunks.empty()) return chunks;
            std::size_t bodySize = 0;
            for (auto const& chunk : chunks) bodySize += 4 + RecordHeader + chooseEncoding(*chunk.data).bytes + 4;
            Bytes journal;
            journal.reserve(JournalHeader + bodySize + 4);
            magic(journal, JournalMagic);
            put32(journal, FormatVersion);
            put32(journal, GeneratorVersion);
            put64(journal, worldSeed);
            put32(journal, static_cast<std::uint32_t>(chunks.size()));
            put32(journal, static_cast<std::uint32_t>(bodySize));
            std::vector<RecordView> records;
            records.reserve(chunks.size());
            for (auto const& chunk : chunks) {
                auto size = RecordHeader + chooseEncoding(*chunk.data).bytes + 4;
                put32(journal, static_cast<std::uint32_t>(size));
                records.push_back({chunk.key, journal.size(), size});
                appendRecord(journal, worldSeed, chunk.key, *chunk.data);
            }
            seal(journal);
            auto temporary = directory / "pending.txn.tmp";
            auto pending = directory / "pending.txn";
            writeFile(temporary, journal);
            renameFile(temporary, pending);
            phase = "CommitUncertain";
            syncDirectory(directory);
            phase = "DurableCheckpointBlocked";
            for (auto const& chunk : chunks) eraseCache(chunk.key);
            checkpoint(journal, records);
            // No fallible allocations/cache population after the durable acknowledgment boundary.
            return chunks;
        } catch (std::exception const& error) {
            blocked = true;
            fail(std::string(phase) + ": " + error.what());
        }
    }
};
#else
// Keep the demo/build portable without pretending an unchecked Windows directory
// flush is durable. World storage currently requires POSIX flock + directory fsync.
struct Store::Impl {
    Impl(std::filesystem::path, std::optional<std::uint64_t>) {
        throw std::runtime_error("World storage requires Linux/macOS durable filesystem primitives; Windows world storage is unsupported");
    }
    std::uint64_t worldSeed = 0;
    std::size_t cacheBytes = 0;
    std::map<ChunkKey, int> cache;
    std::set<ChunkKey> savedKeys;
    ChunkData load(ChunkKey) { throw std::runtime_error("Unsupported world storage platform"); }
    std::vector<Chunk> edit(Brush) { throw std::runtime_error("Unsupported world storage platform"); }
    std::vector<ChunkKey> savedKeysWithin(ChunkKey) const { return {}; }
};
#endif

Store::Store(std::filesystem::path directory, std::optional<std::uint64_t> requestedSeed)
    : impl_(std::make_unique<Impl>(std::move(directory), requestedSeed)) {}
Store::~Store() = default;
std::uint64_t Store::seed() const { return impl_->worldSeed; }
ChunkData Store::load(ChunkKey key) { return impl_->load(key); }
std::vector<Chunk> Store::edit(Brush brush) { return impl_->edit(brush); }
std::vector<ChunkKey> Store::savedKeysWithin(ChunkKey coarse) const { return impl_->savedKeysWithin(coarse); }
std::size_t Store::savedCount() const { return impl_->savedKeys.size(); }
std::size_t Store::cachedBytes() const { return impl_->cacheBytes; }
std::size_t Store::cachedChunks() const { return impl_->cache.size(); }
}
