// Native instrumentation: every intercepted call is forwarded to the real GL driver.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include "core/helpers.hpp"
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/OpenGL.h>
#include <dlfcn.h>
#endif
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "EDAN35/util/settings.cpp"
#include "EDAN35/util/voxel_util.cpp"
#include "EDAN35/util/IntersectionTests.cpp"
#include "EDAN35/util/parametric_shapes.cpp"
#include "EDAN35/project/VoxelVolume.cpp"
#include "EDAN35/world/Generate.hpp"
#include "EDAN35/world/Frontier.hpp"
#include "EDAN35/world/WorldRenderer.hpp"

namespace {
using Clock = std::chrono::steady_clock;
double milliseconds(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
void require(bool value, std::string const& message) {
    if (!value) throw std::runtime_error(message);
}
struct Counters {
    std::uint64_t bytes = 0, image_calls = 0, subimage_calls = 0;
    double upload_cpu_ms = 0, upload_drained_ms = 0;
} counters;
bool isolate_uploads = false;
bool poison_empty_atlas = false;
int framebuffer_pixels = 1000;
PFNGLTEXIMAGE3DPROC real_image;
PFNGLTEXSUBIMAGE3DPROC real_subimage;
PFNGLDRAWELEMENTSPROC real_draw;
std::vector<GLubyte> const* expected = nullptr;
GLuint sampled_texture = 0;
std::uint64_t texture_hash = 0;
std::uint64_t hash(std::vector<GLubyte> const& bytes) {
    std::uint64_t result = 14695981039346656037ull;
    for (auto value : bytes) result = (result ^ value) * 1099511628211ull;
    return result;
}
void APIENTRY image(GLenum target, GLint level, GLint internal, GLsizei w,
                    GLsizei h, GLsizei d, GLint border, GLenum format,
                    GLenum type, void const* data) {
    if (isolate_uploads) glFinish();
    auto start = Clock::now();
    std::vector<GLushort> undefined;
    if (poison_empty_atlas && internal == GL_R16UI && !data) {
        undefined.assign(std::size_t(w) * h * d, 0xffff);
        data = undefined.data();
    }
    real_image(target, level, internal, w, h, d, border, format, type, data);
    counters.upload_cpu_ms += milliseconds(start);
    if (isolate_uploads) { glFinish(); counters.upload_drained_ms += milliseconds(start); }
    if (data) counters.bytes += std::uint64_t(w) * h * d;
    ++counters.image_calls;
}
void APIENTRY subimage(GLenum target, GLint level, GLint x, GLint y, GLint z,
                       GLsizei w, GLsizei h, GLsizei d, GLenum format,
                       GLenum type, void const* data) {
    if (isolate_uploads) glFinish();
    auto start = Clock::now();
    real_subimage(target, level, x, y, z, w, h, d, format, type, data);
    counters.upload_cpu_ms += milliseconds(start);
    if (isolate_uploads) { glFinish(); counters.upload_drained_ms += milliseconds(start); }
    counters.bytes += std::uint64_t(w) * h * d;
    ++counters.subimage_calls;
}
void APIENTRY draw(GLenum mode, GLsizei count, GLenum type, void const* indices) {
    if (expected) {
        GLint bound = 0;
        GLint active = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_3D, &bound);
        sampled_texture = GLuint(bound);
        std::vector<GLubyte> data(expected->size());
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glGetTexImage(GL_TEXTURE_3D, 0, GL_RED, GL_UNSIGNED_BYTE, data.data());
        require(data == *expected, "Real 3D texture readback differs from CPU voxels");
        texture_hash = hash(data);
        glActiveTexture(GLenum(active));
    }
    real_draw(mode, count, type, indices);
}
void checkGL(std::string const& where) {
    auto error = glGetError();
    require(error == GL_NO_ERROR, where + ": GL error " + std::to_string(error));
}
std::string load(std::string const& path) {
    std::ifstream stream(path);
    require(bool(stream), "Cannot read " + path);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
GLuint shader(GLenum type, std::string const& path) {
    auto source = load(path);
    auto text = source.c_str();
    auto object = glCreateShader(type);
    glShaderSource(object, 1, &text, nullptr);
    glCompileShader(object);
    GLint ok = 0;
    glGetShaderiv(object, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[16384]{};
        glGetShaderInfoLog(object, sizeof(log), nullptr, log);
        throw std::runtime_error(path + ": " + log);
    }
    return object;
}
GLuint program(std::string const& stem = "voxel") {
    auto vs = shader(GL_VERTEX_SHADER, std::string(VOXEL_BENCHMARK_SHADER_DIR) + "/" + stem + ".vert");
    auto fs = shader(GL_FRAGMENT_SHADER, std::string(VOXEL_BENCHMARK_SHADER_DIR) + "/" + stem + ".frag");
    auto result = glCreateProgram();
    glAttachShader(result, vs); glAttachShader(result, fs); glLinkProgram(result);
    GLint ok = 0;
    glGetProgramiv(result, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[16384]{};
        glGetProgramInfoLog(result, sizeof(log), nullptr, log);
        throw std::runtime_error(std::string("Shader link: ") + log);
    }
    glDeleteShader(vs); glDeleteShader(fs);
    return result;
}
struct Surface {
#ifdef __APPLE__
    CGLContextObj context = nullptr;
    void* framework = nullptr;
#endif
    GLFWwindow* window = nullptr;
    GLuint framebuffer = 0, color = 0, depth = 0;
    Surface() {
        const int pixels = framebuffer_pixels;
#ifdef __APPLE__
        CGLPixelFormatAttribute attributes[] = {kCGLPFAOpenGLProfile,
            static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_GL4_Core),
            kCGLPFAAccelerated, kCGLPFAAllowOfflineRenderers,
            static_cast<CGLPixelFormatAttribute>(0)};
        CGLPixelFormatObj format = nullptr;
        GLint count = 0;
        require(CGLChoosePixelFormat(attributes, &format, &count) == kCGLNoError && format, "CGL pixel format unavailable");
        auto error = CGLCreateContext(format, nullptr, &context);
        CGLDestroyPixelFormat(format);
        require(error == kCGLNoError && context, "CGL context creation failed");
        require(CGLSetCurrentContext(context) == kCGLNoError, "CGL context activation failed");
        framework = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY | RTLD_GLOBAL);
        require(framework != nullptr, "Cannot load native OpenGL framework");
        require(gladLoadGLLoader([](char const* name) -> void* { return dlsym(RTLD_DEFAULT, name); }) != 0, "GLAD initialization failed");
#else
        glfwSetErrorCallback([](int code, char const* text) { std::cerr << "GLFW " << code << ": " << text << '\n'; });
        require(glfwInit() == GLFW_TRUE, "glfwInit failed");
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        window = glfwCreateWindow(pixels, pixels, "voxel benchmark", nullptr, nullptr);
        require(window != nullptr, "Cannot create native GL 4.1 core context");
        glfwMakeContextCurrent(window);
        glfwSwapInterval(0);
        require(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0, "GLAD initialization failed");
#endif
        glGenFramebuffers(1, &framebuffer); glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glGenTextures(1, &color); glBindTexture(GL_TEXTURE_2D, color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pixels, pixels, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
        glGenRenderbuffers(1, &depth); glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, pixels, pixels);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Incomplete framebuffer");
        glViewport(0, 0, pixels, pixels); glEnable(GL_DEPTH_TEST);
        glClearColor(0.1f, 0.15f, 0.2f, 1.0f);
        real_image = glad_glTexImage3D; glad_glTexImage3D = image;
        real_subimage = glad_glTexSubImage3D; glad_glTexSubImage3D = subimage;
        real_draw = glad_glDrawElements; glad_glDrawElements = draw;
        checkGL("context setup");
    }
    ~Surface() {
        glDeleteRenderbuffers(1, &depth); glDeleteTextures(1, &color);
        glDeleteFramebuffers(1, &framebuffer);
#ifdef __APPLE__
        CGLSetCurrentContext(nullptr); CGLDestroyContext(context);
        dlclose(framework);
#else
        glfwDestroyWindow(window); glfwTerminate();
    #endif
    }
};
std::vector<GLubyte> cpu(VoxelVolume const& volume) {
    std::vector<GLubyte> result(std::size_t(volume.W) * volume.H * volume.D);
    for (int z = 0; z < volume.D; ++z)
        for (int y = 0; y < volume.H; ++y)
            for (int x = 0; x < volume.W; ++x)
                result[x + volume.W * (y + volume.H * z)] = GLubyte(volume.getVoxel(x, y, z));
    return result;
}
void render(VoxelVolume& volume, glm::mat4 const& clip, glm::vec3 camera, bool verify = false) {
    std::vector<GLubyte> data;
    if (verify) { data = cpu(volume); expected = &data; }
    volume.render(glm::mat4(1), clip, camera, false, 1, 1);
    expected = nullptr;
}
std::uint64_t saveImage(std::string const& path) {
    std::vector<GLubyte> image(framebuffer_pixels * framebuffer_pixels * 4);
    glReadPixels(0, 0, framebuffer_pixels, framebuffer_pixels, GL_RGBA, GL_UNSIGNED_BYTE, image.data());
    std::ofstream output(path, std::ios::binary);
    require(bool(output), "Cannot write " + path);
    output.write(reinterpret_cast<char const*>(image.data()), image.size());
    return hash(image);
}
void atlasSmoke(std::ostream& log) {
    world::LevelAtlas atlas;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // Allocation without data may legally return nonzero GPU memory.
    poison_empty_atlas = true;
    atlas.init();
    poison_empty_atlas = false;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    std::vector<GLushort> entries(world::PageSize * world::PageSize * world::PageSize * world::LevelCount);
    glBindTexture(GL_TEXTURE_3D, atlas.texture());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_3D, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, entries.data());
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    require(std::all_of(entries.begin(), entries.end(), [](auto entry) { return entry == 0; }),
            "New page atlas contains nonempty entries");
    checkGL("page atlas initialization");
    log << "smoke,page-atlas-initialization=pass\n";
}

void smoke(GLuint shader_program, bool strict, std::ostream& log, std::string const& prefix) {
    glm::vec3 camera(2, 1.7f, 2.5f);
    auto clip = glm::perspective(glm::radians(40.0f), 1.0f, 0.1f, 20.0f) * glm::lookAt(camera, glm::vec3(0.5f), glm::vec3(0,1,0));
    auto volume = std::make_unique<VoxelVolume>(17, 9, 5, Transform());
    volume->setProgram(shader_program);
    auto verify = [&](char const* stage, bool clean = false) {
        counters = {};
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        render(*volume, clip, camera, true);
        glFinish(); checkGL(stage);
        if (strict && clean) require(counters.bytes == 0 && counters.image_calls == 0 && counters.subimage_calls == 0, std::string(stage) + ": unchanged volume uploaded");
        log << "smoke," << stage << ",bytes=" << counters.bytes << ",image_calls=" << counters.image_calls << ",subimage_calls=" << counters.subimage_calls << ",texture_hash=" << texture_hash << '\n';
    };
    verify("initial");
    if (strict) require(counters.image_calls == 2 && counters.bytes == 17 * 9 * 5 + 3 * 2, "Initial material and occupancy upload not exactly once");
    verify("unchanged", true);
    volume->setVoxel(0,0,0,13); volume->setVoxel(16,8,4,201);
    volume->setVoxel(3,7,2,77); volume->setVoxel(15,1,2,88);
    verify("edge-disjoint");
    if (strict) require(counters.image_calls == 0 && counters.bytes < 17 * 9 * 5, "Disjoint edits uploaded full volume");
    volume->setVoxel(3,7,2,77);
    verify("same-value", true);
    volume->updateVoxels([](int, int, int, GLubyte previous) { return previous; });
    verify("callback-noop", true);
    volume->updateVoxels([&volume](int x,int y,int z,GLubyte previous) {
        if (x==0 && y==0 && z==0) volume->setVoxel(x,y,z,99);
        return previous;
    });
    require(volume->getVoxel(0,0,0)==13, "Callback return did not replace its reentrant mutation");
    verify("callback-reentrant");
    volume->updateVoxels([](int x, int y, int z, GLubyte) { return GLubyte(1 + (x + 3*y + 7*z) % 254); });
    verify("callback-full");
    std::vector<std::vector<std::vector<GLubyte>>> imported(17, std::vector<std::vector<GLubyte>>(9, std::vector<GLubyte>(5)));
    std::vector<std::vector<GLubyte*>> rows(17, std::vector<GLubyte*>(9));
    std::vector<GLubyte**> planes(17);
    for (int x=0; x<17; ++x) { planes[x] = rows[x].data(); for (int y=0; y<9; ++y) { rows[x][y] = imported[x][y].data(); for (int z=0; z<5; ++z) imported[x][y][z] = GLubyte(1+(x*11+y*3+z)%254); } }
    volume->setVolumeData3D(planes.data()); verify("import");
    volume->setVolumeData3D(planes.data()); verify("import-repeat", true);
    volume->setSphere(glm::vec3(0.5f), 0.18f, 222); verify("sphere");
    volume->setSphere(glm::vec3(0.0f,0.4f,0.5f), 0.12f, 111); verify("sphere-boundary-add");
    volume->setSphere(glm::vec3(0.0f,0.4f,0.5f), 0.12f, 0); verify("sphere-boundary-remove");
    volume->cleanVoxel(); verify("clear");
    volume->cleanVoxel(); verify("clear-repeat", true);
    volume->setVoxel(8,4,2,255); verify("final-visible");
    auto image_hash = saveImage(prefix + "-smoke.rgba");
    log << "smoke,image_hash=" << image_hash << '\n';
    if (strict) {
        auto before = cpu(*volume);
        for (auto p : {glm::ivec3(-1,1,1), glm::ivec3(17,0,0), glm::ivec3(0,-1,1), glm::ivec3(0,9,0), glm::ivec3(0,0,-1), glm::ivec3(0,0,5)}) {
            require(volume->getVoxel(p) == -1, "Invalid per-axis read accepted");
            require(!volume->setVoxel(p,42), "Invalid per-axis write accepted");
        }
        require(before == cpu(*volume), "Invalid coordinates modified voxels");
        verify("invalid-edits", true);
        GLuint pbo = 0; glGenBuffers(1,&pbo); glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 4096, nullptr, GL_STATIC_DRAW);
        const GLenum keys[] = {GL_UNPACK_ALIGNMENT,GL_UNPACK_ROW_LENGTH,GL_UNPACK_IMAGE_HEIGHT,GL_UNPACK_SKIP_PIXELS,GL_UNPACK_SKIP_ROWS,GL_UNPACK_SKIP_IMAGES};
        const GLint poison[] = {8,31,23,2,3,1};
        for (int i=0;i<6;++i) glPixelStorei(keys[i],poison[i]);
        volume->setVoxel(16,8,4,99); verify("poisoned-unpack");
        for (int i=0;i<6;++i) { GLint value=0; glGetIntegerv(keys[i],&value); require(value==poison[i], "Unpack state not restored"); }
        GLint bound=0; glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&bound); require(GLuint(bound)==pbo,"Unpack PBO binding not restored");
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0); glDeleteBuffers(1,&pbo);
        for (int i=0;i<6;++i) glPixelStorei(keys[i],i==0 ? 4 : 0);
        auto texture = sampled_texture;
        glActiveTexture(GL_TEXTURE1);
        // Explicit upload rebinds both persistent textures without changing data.
        require(volume->upload() == 0, "Clean explicit upload transferred bytes");
        glActiveTexture(GL_TEXTURE1);
        GLint coarse = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_3D, &coarse);
        glActiveTexture(GL_TEXTURE0);
        require(coarse != 0 && glIsTexture(GLuint(coarse)), "Occupancy texture missing");
        require(glIsTexture(texture)==GL_TRUE,"Volume texture not alive after render");
        volume.reset();
        require(glIsTexture(texture)==GL_FALSE,"Volume texture not deleted by destructor");
        require(glIsTexture(GLuint(coarse))==GL_FALSE,"Occupancy texture not deleted by destructor");
        log << "smoke,strict-bounds-unpack-lifetime=pass\n";
    }
}
void raycastSmoke(bool strict, std::ostream& log) {
    VoxelVolume volume(17,9,5,Transform());
    bool correct = true;
    int transformed = 0;
    for (auto transform : {Transform(),Transform().translate(4,-2,3).rotateAroundY(0.37f).rotateAroundZ(0.23f).scale(glm::vec3(2,0.6f,1.4f))}) {
        volume.transform = transform;
        volume.updateVoxels([](int,int,int,GLubyte) { return GLubyte(9); });
        auto directionToWorld = [&](glm::vec3 direction) {
            return glm::normalize(glm::vec3(transform.getMatrix()*glm::vec4(direction,0)));
        };
        for (int axis=0;axis<3;++axis) for (int side : {-1,1}) {
            glm::vec3 origin(0.5f), direction(0);
            origin[axis] = side<0 ? -1.0f : 2.0f;
            direction[axis] = float(-side);
            auto wanted = volume.size()/2;
            wanted[axis] = side<0 ? 0 : volume.size()[axis]-1;
            auto hit = volume.raycast(transform.apply(origin),directionToWorld(direction));
            bool valid = !hit.miss && hit.material==9 && hit.index==wanted;
            correct = correct && valid;
            log << "raycast,transformed=" << transformed << ",axis=" << axis << ",side=" << side << ",miss=" << hit.miss;
            if (!hit.miss) log << ",index=" << hit.index.x << ':' << hit.index.y << ':' << hit.index.z;
            log << ",correct=" << valid << '\n';
        }
        auto inside = volume.raycast(transform.apply(glm::vec3(0.5f)),directionToWorld(glm::vec3(1,0,0)));
        bool inside_ok = !inside.miss && inside.material==9 && inside.index==volume.size()/2;
        auto away = volume.raycast(transform.apply(glm::vec3(2,0.5f,0.5f)),directionToWorld(glm::vec3(1,0,0)));
        if (!transformed) {
            auto closed_corner = volume.isInside(glm::vec3(1));
            bool closed_ok = !closed_corner.miss && closed_corner.material==9 && closed_corner.index==volume.size()-glm::ivec3(1);
            log << "raycast,closed_corner_correct=" << closed_ok << '\n';
            correct = correct && closed_ok;
        }
        volume.cleanVoxel();
        auto empty = volume.raycast(transform.apply(glm::vec3(-1,0.5f,0.5f)),directionToWorld(glm::vec3(1,0,0)));
        log << "raycast,transformed=" << transformed << ",inside_correct=" << inside_ok << ",away_miss=" << away.miss << ",empty_miss=" << empty.miss << '\n';
        correct = correct && inside_ok && away.miss && empty.miss;
        ++transformed;
    }
    if (strict) require(correct, "Six-face raycast/editing contract failed");
}

struct Options {
    int volumes=100, dynamic_volumes=4, frames=100, warmup=10;
    bool strict=false;
    std::string label="candidate", scenario="all", output="voxel-benchmark";
    // Mountains only.
    int pairs=1;
    std::uint64_t seed = world::DefaultSeed;
    std::string pose = "ridge";
    int width = 0, height = 0;
    bool water = true;
};
Options options(int argc, char** argv) {
    Options result;
    for (int i=1;i<argc;++i) {
        std::string arg=argv[i];
        if (arg=="--strict") { result.strict=true; continue; }
        if (arg=="--isolate-uploads") { isolate_uploads=true; continue; }
        require(i+1<argc,"Missing value for "+arg);
        std::string value=argv[++i];
        if (arg=="--label") result.label=value;
        else if (arg=="--scenario") result.scenario=value;
        else if (arg=="--output") result.output=value;
        else if (arg=="--volumes") result.volumes=std::stoi(value);
        else if (arg=="--dynamic-volumes") result.dynamic_volumes=std::stoi(value);
        else if (arg=="--frames") result.frames=std::stoi(value);
        else if (arg=="--warmup") result.warmup=std::stoi(value);
        else if (arg=="--pixels") framebuffer_pixels=std::stoi(value);
        else if (arg=="--pairs") result.pairs=std::stoi(value);
        else if (arg=="--pose") result.pose=value;
        else if (arg=="--width") result.width=std::stoi(value);
        else if (arg=="--height") result.height=std::stoi(value);
        else if (arg=="--water") result.water=std::stoi(value)!=0;
        else if (arg=="--seed") result.seed=std::stoull(value);
        else throw std::runtime_error("Unknown option "+arg);
    }
    require(result.volumes>0 && result.dynamic_volumes>0 && result.frames>0 && result.warmup>=0,"Invalid workload sizes");
    require(framebuffer_pixels>0 && framebuffer_pixels<=4096,"Invalid framebuffer size");
    require(result.pose=="spawn" || result.pose=="ridge" || result.pose=="valley" || result.pose=="underwater" || result.pose=="summit","Invalid mountains pose");
    require(result.pairs>0,"Invalid pair count");
    require(result.scenario=="all" || result.scenario=="mountains" || result.scenario=="smoke" || result.scenario=="static" || result.scenario=="local" || result.scenario=="scattered" || result.scenario=="full","Invalid scenario");
    if (result.width == 0) result.width = framebuffer_pixels;
    if (result.height == 0) result.height = framebuffer_pixels;
    require(result.width>0 && result.width<=4096 && result.height>0 && result.height<=4096,"Invalid mountains framebuffer size");
    return result;
}
// ----------------------------------------------------------------- mountains --
// Minimal PNG (stored deflate) so every pose leaves an inspectable image without extra tooling.
std::uint32_t crc32(std::uint8_t const* data, std::size_t size) {
    static std::array<std::uint32_t, 256> const table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t n = 0; n < 256; ++n) {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[n] = c;
        }
        return t;
    }();
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}
void writePng(std::string const& path, int width, int height, std::vector<GLubyte> const& rgbaBottomUp) {
    std::vector<std::uint8_t> raw;
    raw.reserve(std::size_t(height) * (1 + 3 * std::size_t(width)));
    for (int y = height - 1; y >= 0; --y) {
        raw.push_back(0);
        auto const* row = &rgbaBottomUp[std::size_t(y) * std::size_t(width) * 4];
        for (int x = 0; x < width; ++x) raw.insert(raw.end(), row + x * 4, row + x * 4 + 3);
    }
    std::vector<std::uint8_t> zlib{0x78, 0x01};
    std::uint32_t a = 1, b = 0;
    for (auto v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
    std::size_t offset = 0;
    do {
        std::size_t len = std::min<std::size_t>(65535, raw.size() - offset);
        bool last = offset + len == raw.size();
        zlib.push_back(last ? 1 : 0);
        zlib.push_back(std::uint8_t(len & 0xFF)); zlib.push_back(std::uint8_t((len >> 8) & 0xFF));
        zlib.push_back(std::uint8_t(~len & 0xFF)); zlib.push_back(std::uint8_t((~len >> 8) & 0xFF));
        zlib.insert(zlib.end(), raw.begin() + std::ptrdiff_t(offset), raw.begin() + std::ptrdiff_t(offset + len));
        offset += len;
    } while (offset < raw.size());
    auto be32 = [](std::vector<std::uint8_t>& v, std::uint32_t x) {
        for (int shift = 24; shift >= 0; shift -= 8) v.push_back(std::uint8_t((x >> shift) & 0xFF));
    };
    be32(zlib, (b << 16) | a);
    std::ofstream out(path, std::ios::binary);
    require(bool(out), "Cannot write " + path);
    auto chunk = [&](char const* type, std::vector<std::uint8_t> const& body) {
        std::vector<std::uint8_t> block(type, type + 4);
        block.insert(block.end(), body.begin(), body.end());
        std::vector<std::uint8_t> head;
        be32(head, std::uint32_t(body.size()));
        be32(block, crc32(block.data(), block.size()));
        out.write(reinterpret_cast<char const*>(head.data()), std::streamsize(head.size()));
        out.write(reinterpret_cast<char const*>(block.data()), std::streamsize(block.size()));
    };
    std::uint8_t const signature[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    out.write(reinterpret_cast<char const*>(signature), sizeof(signature));
    std::vector<std::uint8_t> header;
    be32(header, std::uint32_t(width));
    be32(header, std::uint32_t(height));
    header.insert(header.end(), {8, 2, 0, 0, 0});
    chunk("IHDR", header);
    chunk("IDAT", zlib);
    chunk("IEND", {});
}

glm::dvec3 metres(world::WorldPosition const& p) {
    return glm::dvec3(double(p.anchor.x), double(p.anchor.y), double(p.anchor.z)) * double(world::ChunkSpan) + p.offset;
}
world::WorldPosition worldAt(glm::dvec3 m) {
    auto p = world::normalizedPosition(world::ChunkKey{0, 0, 0, 0}, m);
    require(bool(p), "Pose is not representable");
    return *p;
}
// Nearest lattice column around `center` within `reach` metres whose height passes `accept`.
template <class Accept>
std::optional<glm::dvec3> nearestColumn(std::uint64_t seed, glm::dvec2 center, double reach, double step, Accept&& accept) {
    std::optional<glm::dvec3> best;
    double bestD2 = reach * reach + 1;
    for (double dz = -reach; dz <= reach; dz += step)
        for (double dx = -reach; dx <= reach; dx += step) {
            double d2 = dx * dx + dz * dz;
            if (d2 > reach * reach || d2 >= bestD2) continue;
            double x = center.x + dx, z = center.y + dz;
            double h = world::terrainHeight(seed, x, z);
            if (!accept(h)) continue;
            bestD2 = d2;
            best = glm::dvec3(x, h, z);
        }
    return best;
}
// Highest (or lowest) lattice column around `center` within `reach` metres.
glm::dvec3 extremeColumn(std::uint64_t seed, glm::dvec2 center, double reach, double step, bool highest) {
    glm::dvec3 best(center.x, highest ? -1e300 : 1e300, center.y);
    for (double dz = -reach; dz <= reach; dz += step)
        for (double dx = -reach; dx <= reach; dx += step) {
            if (dx * dx + dz * dz > reach * reach) continue;
            double x = center.x + dx, z = center.y + dz;
            double h = world::terrainHeight(seed, x, z);
            if (highest ? h > best.y : h < best.y) best = glm::dvec3(x, h, z);
        }
    return best;
}
struct Pose {
    world::WorldPosition eye, target;
};
Pose mountainPose(std::string const& name, std::uint64_t seed) {
    using world::SeaLevel;
    auto height = [seed](double x, double z) { return world::terrainHeight(seed, x, z); };
    auto spawn = metres(world::spawnPosition(seed));
    glm::dvec2 spawnXZ(spawn.x, spawn.z);
    if (name == "spawn") return {worldAt(spawn), worldAt(metres(world::spawnTarget(seed)))};
    // Pinned XZ from the GeneratorVersion 2 spawn ridge; heights follow the current generator so the
    // viewpoint stays above ground across relief changes.
    if (name == "ridge")
        return {worldAt(glm::dvec3(900, height(900, -1200) + 30, -1200)),
                worldAt(glm::dvec3(-1450, height(-1450, -1550) + 10, -1550))};
    if (name == "valley") {
        auto ridge = nearestColumn(seed, spawnXZ, 3000, 50, [](double h) { return h > 500; });
        require(bool(ridge), "No ridge above 500 m within 3 km of spawn");
        glm::dvec2 toSpawn = glm::normalize(spawnXZ - glm::dvec2(ridge->x, ridge->z));
        std::optional<glm::dvec3> floor;
        for (double d = 10; d <= 3000 && !floor; d += 10) {
            double x = ridge->x + toSpawn.x * d, z = ridge->z + toSpawn.y * d;
            double h = height(x, z);
            if (h <= ridge->y - 300) floor = glm::dvec3(x, h, z);
        }
        require(bool(floor), "No valley floor 300 m below the nearest ridge");
        // The valley axis runs across the slope gradient; look down-valley.
        double gx = (height(floor->x + 2, floor->z) - height(floor->x - 2, floor->z)) / 4;
        double gz = (height(floor->x, floor->z + 2) - height(floor->x, floor->z - 2)) / 4;
        glm::dvec2 axis(-gz, gx);
        axis = glm::length(axis) < 1e-6 ? toSpawn : glm::normalize(axis);
        auto ahead = [&](glm::dvec2 dir) {
            double sum = 0;
            for (double d = 50; d <= 300; d += 50) sum += height(floor->x + dir.x * d, floor->z + dir.y * d);
            return sum / 6;
        };
        if (ahead(-axis) < ahead(axis)) axis = -axis;
        glm::dvec3 eye(floor->x, std::max(floor->y, SeaLevel) + 1.8, floor->z);
        return {worldAt(eye), worldAt(glm::dvec3(eye.x + axis.x * 300, eye.y - 15, eye.z + axis.y * 300))};
    }
    if (name == "underwater") {
        // Prefer the 400 m ring around spawn; a spawn that sits inland falls back to the nearest seabed within 3 km.
        auto deep = nearestColumn(seed, spawnXZ, 400, 5, [](double h) { return h < SeaLevel - 7; });
        if (!deep) deep = nearestColumn(seed, spawnXZ, 3000, 10, [](double h) { return h < SeaLevel - 7; });
        if (!deep) deep = nearestColumn(seed, spawnXZ, 12000, 25, [](double h) { return h < SeaLevel - 7; });
        require(bool(deep), "No seabed 7 m below sea level within 12 km of spawn");
        glm::dvec3 eye(deep->x, SeaLevel - 5, deep->z);
        glm::dvec2 shore = glm::normalize(spawnXZ - glm::dvec2(eye.x, eye.z));
        return {worldAt(eye), worldAt(glm::dvec3(eye.x + shore.x * 100, eye.y, eye.z + shore.y * 100))};
    }
    require(name == "summit", "Mountains scenario needs --pose spawn|ridge|valley|underwater|summit");
    auto coarse = extremeColumn(seed, spawnXZ, 3000, 50, true);
    auto summit = extremeColumn(seed, glm::dvec2(coarse.x, coarse.z), 50, 5, true);
    auto sea = nearestColumn(seed, glm::dvec2(summit.x, summit.z), 6000, 50, [](double h) { return h < SeaLevel; });
    require(bool(sea), "No sea within 6 km of the summit");
    // The 5 m search grid can miss a neighbouring column up to ~5 m higher on alpine slopes. Look
    // towards the nearest sea but no steeper than 15 degrees down so the horizon stays in frame.
    glm::dvec3 eye(summit.x, summit.y + 12, summit.z);
    double run = glm::length(glm::dvec2(sea->x, sea->z) - glm::dvec2(summit.x, summit.z));
    double targetY = std::max(SeaLevel, eye.y - run * std::tan(glm::radians(15.0)));
    return {worldAt(eye), worldAt(glm::dvec3(sea->x, targetY, sea->z))};
}

struct Resident {
    std::uint8_t uniform = world::Air;
    std::optional<std::uint16_t> slot;
};
struct LevelStats {
    int keys = 0, bricks = 0, uniformSolid = 0, uniformAir = 0, drawn = 0, drawnBricks = 0, drawnSolid = 0, drawnAir = 0;
};
struct FrameTimes {
    double opaqueGpu, compositeGpu, opaqueSubmit, compositeSubmit, wall;
    double cpuSubmit() const { return opaqueSubmit + compositeSubmit; }
};
struct ColumnStats {
    double mean, p99;
};
ColumnStats columnStats(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    double sum = 0;
    for (auto v : values) sum += v;
    return {sum / double(values.size()), values[std::size_t(std::ceil(0.99 * double(values.size()))) - 1]};
}

// Exact synthetic rays exercise the production renderer, including its MRT/composite contract.
// Run outside timing, before generated terrain populates the page tables.
void waterSmoke(world::WorldRenderer& renderer, std::array<glm::vec3, 256> const& palette,
                GLuint target, int width, int height, std::string const& prefix, std::ostream& log) {
    using namespace world;
    constexpr float NoHit = 1e30f;
    auto slot = renderer.bricks().allocate();
    require(bool(slot), "No brick for water GPU cases");
    ChunkData data{};
    auto slices = [&](int axis, auto material) {
        for (int z = 0; z < ChunkSize; ++z)
            for (int y = 0; y < ChunkSize; ++y)
                for (int x = 0; x < ChunkSize; ++x) {
                    int coordinate = axis == 0 ? x : axis == 1 ? y : z;
                    data[index(x, y, z)] = std::uint8_t(material(coordinate));
                }
        renderer.bricks().upload(*slot, data);
    };
    auto domain = [](int level, float origin, float span) {
        return LevelUniforms{.level = level, .regionOrigin = glm::vec3(origin, 0, 0), .chunkSpan = span,
            .regionSize = glm::ivec3(1), .pageOrigin = glm::ivec3(0),
            .holeLo = glm::ivec3(0), .holeHi = glm::ivec3(0), .topRow = 0};
    };
    std::vector<LevelUniforms> domains{domain(0, 0, 32)};
    renderer.table(0).set({0, 0, 0, 0}, std::uint16_t(256 + *slot));
    struct Sample {
        std::array<float, 4> march{};
        std::array<GLubyte, 3> color{};
        std::array<GLubyte, 4> composite{};
    };
    auto run = [&](char const* name, glm::vec3 eye, glm::vec3 direction, std::array<float, 4> expectedMarch) {
        std::array<Sample, 2> samples;
        for (int acceleration = 0; acceleration < 2; ++acceleration) {
            glm::mat4 clipToWorld(0);
            clipToWorld[3] = glm::vec4(eye + direction, 1);
            FrameUniforms frame{.worldToClip = glm::mat4(1), .clipToWorld = clipToWorld,
                .cameraPosition = eye, .sunDirection = glm::normalize(glm::vec3(0.35f, 0.8f, 0.45f)),
                .acceleration = acceleration != 0, .palette = &palette, .width = 1, .height = 1};
            glBindFramebuffer(GL_FRAMEBUFFER, target);
            renderer.beginFrame(frame);
            for (auto const& d : domains) renderer.drawLevel(d);
            renderer.march();
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadBuffer(GL_COLOR_ATTACHMENT1);
            glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, samples[acceleration].march.data());
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glReadPixels(0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, samples[acceleration].color.data());
            renderer.composite();
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, samples[acceleration].composite.data());
            checkGL(std::string("water GPU case ") + name);
            for (int channel = 0; channel < 4; ++channel) {
                float actual = samples[acceleration].march[channel];
                float expectedValue = expectedMarch[channel];
                require(expectedValue == NoHit ? actual >= 1e29f : std::abs(actual - expectedValue) < 0.0001f,
                    std::string(name) + ": channel " + std::to_string(channel) + " expected " +
                    std::to_string(expectedValue) + ", got " + std::to_string(actual));
            }
        }
        require(std::memcmp(samples[0].march.data(), samples[1].march.data(), sizeof(samples[0].march)) == 0 &&
                samples[0].color == samples[1].color && samples[0].composite == samples[1].composite,
                std::string(name) + ": acceleration changed water, opaque colour/depth or composite");
        log << "water_gpu," << name << ",opaque_m=" << samples[1].march[0]
            << ",water_m=" << samples[1].march[1] << ",boundary_m=" << samples[1].march[2]
            << ",face=" << samples[1].march[3] << ",parity=exact\n";
        return samples[1];
    };
    glm::vec3 eye(0.5f), forward(1, 0, 0);
    slices(0, [](int x) { return x == 12 ? Stone : Air; });
    auto dry = run("dry-solid", eye, forward, {11.5f, 0, NoHit, 0});
    slices(0, [](int x) { return x == 12 ? Stone : ((x >= 2 && x < 4) || (x >= 6 && x < 10)) ? Water : Air; });
    auto wet = run("separated-water-air-intervals", eye, forward, {11.5f, 6, 1.5f, 2});
    require(dry.composite != wet.composite, "Water did not change the production composite");
    run("underwater-exit-reentry", {2.5f, 0.5f, 0.5f}, forward, {9.5f, 5.5f, 1.5f, -2});
    slices(0, [](int x) { return x == 1 ? Stone : ((x >= 2 && x < 4) || (x >= 6 && x < 10)) ? Water : Air; });
    run("solid-occludes-all-water", eye, forward, {0.5f, 0, NoHit, 0});
    run("reverse-side-face", {12.5f, 0.5f, 0.5f}, -forward, {10.5f, 6, 2.5f, 1});
    slices(0, [](int x) { return x == 8 ? Stone : ((x >= 2 && x < 4) || (x >= 6 && x < 10)) ? Water : Air; });
    run("solid-clips-second-water-interval", eye, forward, {7.5f, 4, 1.5f, 2});
    slices(0, [](int x) { return x == 12 ? Stone : x < 8 ? Water : Air; });
    run("homogeneous-water-cell-exit", eye, forward, {11.5f, 7.5f, 7.5f, -2});
    slices(0, [](int x) { return x == 3 ? Stone : Water; });
    run("underwater-solid-no-interface", {2.5f, 0.5f, 0.5f}, forward, {0.5f, 0.5f, NoHit, -7});
    slices(1, [](int y) { return y == 12 ? Stone : y < 8 ? Water : Air; });
    run("underwater-upward-exit", eye, {0, 1, 0}, {11.5f, 7.5f, 7.5f, -4});
    slices(0, [](int) { return Water; });
    run("frontier-ends-water-not-infinite-ocean", eye, forward, {NoHit, 31.5f, 31.5f, -2});
    // Coarse water is nearer than the fine brick: level-index order would see its solid first.
    domains = {domain(0, 64, 32), domain(1, 0, 64)};
    renderer.table(1).set({0, 0, 0, 1}, Water);
    slices(0, [](int x) { return x == 24 ? Stone : (x >= 8 && x < 16) ? Water : Air; });
    run("partial-lod-coarse-before-fine", eye, forward, {87.5f, 71.5f, 63.5f, -2});
    glm::vec3 oblique = glm::normalize(glm::vec3(1, 0.25f, 0.125f));
    float metresPerX = 1.0f / oblique.x;
    run("oblique-partial-lod-order", eye, oblique,
        {87.5f * metresPerX, 71.5f * metresPerX, 63.5f * metresPerX, -2});
    slices(0, [](int x) { return x == 24 ? Stone : x < 8 ? Water : Air; });
    run("water-continuous-across-lod", eye, forward, {87.5f, 71.5f, 71.5f, -2});
    run("oblique-continuous-lod-water", eye, oblique,
        {87.5f * metresPerX, 71.5f * metresPerX, 71.5f * metresPerX, -2});
    domains[0].regionOrigin.x = 96;
    run("missing-lod-span-is-air", eye, forward, {119.5f, 71.5f, 63.5f, -2});
    // A finite voxel tank with an editable-looking air tunnel and opaque chequered floor.
    // These captures expose side faces and an underwater upward view, not a sea-plane proxy.
    domains = {domain(0, 0, 32)};
    for (int z = 0; z < ChunkSize; ++z)
        for (int y = 0; y < ChunkSize; ++y)
            for (int x = 0; x < ChunkSize; ++x) {
                bool tank = x >= 4 && x < 28 && z >= 4 && z < 28 && y < 16;
                bool tunnel = y >= 6 && y < 10 && z >= 12 && z < 20;
                data[index(x, y, z)] = y == 0 ? ((x / 4 + z / 4) % 2 ? Sand : Stone) :
                    tank && !tunnel ? Water : Air;
            }
    renderer.bricks().upload(*slot, data);
    auto capture = [&](char const* name, glm::vec3 camera, glm::vec3 look, glm::vec3 up) {
        auto clip = glm::perspective(glm::radians(65.0f), float(width) / float(height), 0.05f, 200.0f) *
                    glm::lookAt(camera, look, up);
        FrameUniforms frame{.worldToClip = clip, .clipToWorld = glm::inverse(clip), .cameraPosition = camera,
            .sunDirection = glm::normalize(glm::vec3(0.35f, 0.8f, 0.45f)), .acceleration = true,
            .palette = &palette, .width = width, .height = height};
        glBindFramebuffer(GL_FRAMEBUFFER, target);
        renderer.beginFrame(frame);
        renderer.drawLevel(domains.front());
        renderer.march();
        renderer.composite();
        std::vector<GLubyte> rgba(std::size_t(width) * height * 4);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        writePng(prefix + "-" + name + ".png", width, height, rgba);
        checkGL(std::string("water capture ") + name);
    };
    capture("water-side-carved", {40, 10, 22}, {16, 8, 16}, {0, 1, 0});
    capture("water-underwater-up", {16, 12, 16}, {16, 24, 16}, {0, 0, -1});
    renderer.table(0).set({0, 0, 0, 0}, 0);
    renderer.table(1).set({0, 0, 0, 1}, 0);
    renderer.bricks().release(*slot);
    glBindFramebuffer(GL_FRAMEBUFFER, target);
}

void mountainsBenchmark(Options const& opts, std::string const& prefix, std::ostream& csv, std::ostream& log) {
    using namespace world;
    constexpr int MaxChunkDelta = 1 << 24;
    std::string const& poseName = opts.pose;
    int const width = opts.width, height = opts.height;
    std::size_t const pixels = std::size_t(width) * std::size_t(height);

    auto pose = mountainPose(poseName, opts.seed);
    auto anchor = pose.eye.anchor;
    auto targetRelative = relativeOrigin(pose.target.anchor, anchor, MaxChunkDelta);
    require(bool(targetRelative), "Pose target is too far from the eye");
    glm::vec3 const eye(pose.eye.offset);
    glm::vec3 const lookAt = *targetRelative + glm::vec3(pose.target.offset);
    auto worldToClip = glm::perspective(glm::radians(65.0f), float(width) / float(height), 0.05f, 40000.0f) *
                       glm::lookAt(eye, lookAt, glm::vec3(0, 1, 0));

    // The composite writes into this target; the renderer keeps its own colour/distance MRT.
    GLuint target = 0, targetColor = 0;
    glGenFramebuffers(1, &target);
    glBindFramebuffer(GL_FRAMEBUFFER, target);
    glGenTextures(1, &targetColor);
    glBindTexture(GL_TEXTURE_2D, targetColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, targetColor, 0);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Incomplete mountains target");
    glBindTexture(GL_TEXTURE_2D, 0);

    ShaderProgramManager programs;
    WorldRenderer renderer;
    renderer.init(programs);
    auto palette = worldPalette();
    checkGL("mountains renderer init");
    waterSmoke(renderer, palette, target, width, height, prefix, log);
    log << "mountains,render_mode=" << (opts.water ? "voxel-water-composite" : "opaque-only-no-composite (sky/fog/water shading disabled)") << '\n';

    // Cold start: every residency target is generated and uploaded synchronously, then the
    // level tables are filled from the drawn set exactly as WorldApp::refreshDrawn does.
    counters = {};
    auto coldStart = Clock::now();
    auto targets = residencyTargets(anchor, ShellRadius);
    std::map<ChunkKey, Resident> resident;
    std::array<LevelStats, LevelCount> levels{};
    std::size_t uploadedBytes = 0;
    for (auto key : targets) {
        Resident r;
        auto& stats = levels[key.level];
        ++stats.keys;
        if (auto trivial = trivialUniform(opts.seed, key)) {
            r.uniform = *trivial;
        } else {
            auto data = generateChunk(opts.seed, key);
            std::uint8_t value = Air;
            if (isUniform(data, value)) {
                r.uniform = value;
            } else {
                auto slot = renderer.bricks().allocate();
                require(bool(slot), "Brick pool exhausted after " + std::to_string(renderer.bricks().used()) + " bricks");
                uploadedBytes += renderer.bricks().upload(*slot, data);
                r.slot = slot;
            }
        }
        if (r.slot) ++stats.bricks;
        else if (r.uniform == Air) ++stats.uniformAir;
        else ++stats.uniformSolid;
        resident.emplace(key, r);
    }
    double const generateUploadMs = milliseconds(coldStart);
    auto entryOf = [](Resident const& r) -> std::uint16_t {
        if (r.slot) return std::uint16_t(256 + *r.slot);
        return r.uniform == Air ? 0 : r.uniform;
    };
    auto drawn = drawnSet(anchor, ShellRadius, [&](ChunkKey k) { return resident.contains(k); });
    std::set<ChunkKey> drawnKeys(drawn.begin(), drawn.end());
    std::array<int, LevelCount> topRow;
    topRow.fill(-1);
    for (int level = 0; level < LevelCount; ++level) {
        auto r = region(anchor, level, ShellRadius);
        require(bool(r), "Level region is not representable");
        r->each([&](ChunkKey k) {
            std::uint16_t entry = 0;
            if (drawnKeys.contains(k)) {
                auto const& res = resident.at(k);
                entry = entryOf(res);
                auto& stats = levels[level];
                ++stats.drawn;
                if (res.slot) ++stats.drawnBricks;
                else if (res.uniform == Air) ++stats.drawnAir;
                else ++stats.drawnSolid;
            }
            if (entry != 0) topRow[level] = std::max(topRow[level], int(k.y - r->lo.y));
            renderer.table(level).set(k, entry);
        });
    }
    std::array<bool, LevelCount> holeValid;
    holeValid.fill(true);
    holeValid[0] = false;
    for (auto key : drawn) {
        if (key.level == 0) continue;
        auto finer = region(anchor, key.level - 1, ShellRadius);
        auto first = childOf(key, 0), last = childOf(key, 7);
        if (!finer || !first || !last) continue;
        if (finer->contains(*first) && finer->contains(*last)) holeValid[key.level] = false;
    }
    glFinish();
    double const coldStartMs = milliseconds(coldStart);
    auto const coldCounters = counters;
    checkGL("mountains cold start");

    std::array<std::optional<LevelUniforms>, LevelCount> uniforms;
    for (int level = 0; level < LevelCount; ++level) {
        auto r = region(anchor, level, ShellRadius);
        if (!r) continue;
        auto origin = relativeOrigin(r->lo, anchor, MaxChunkDelta);
        if (!origin) continue;
        LevelUniforms u{
            .level = level,
            .regionOrigin = *origin,
            .chunkSpan = spanAt(level),
            .regionSize = glm::ivec3(int(r->hi.x - r->lo.x + 1), int(r->hi.y - r->lo.y + 1), int(r->hi.z - r->lo.z + 1)),
            .pageOrigin = LevelTable::texel(r->lo),
            .holeLo = glm::ivec3(0),
            .holeHi = glm::ivec3(0),
            .topRow = topRow[level],
        };
        if (holeValid[level]) {
            auto finer = *region(anchor, level - 1, ShellRadius);
            u.holeLo = glm::ivec3(int(finer.lo.x / 2 - r->lo.x), int(finer.lo.y / 2 - r->lo.y), int(finer.lo.z / 2 - r->lo.z));
            u.holeHi = glm::ivec3(int((finer.hi.x + 1) / 2 - r->lo.x), int((finer.hi.y + 1) / 2 - r->lo.y), int((finer.hi.z + 1) / 2 - r->lo.z));
        }
        uniforms[level] = u;
    }

    FrameUniforms frame{
        .worldToClip = worldToClip,
        .clipToWorld = glm::inverse(worldToClip),
        .cameraPosition = eye,
        .sunDirection = glm::normalize(glm::vec3(0.35f, 0.8f, 0.45f)),
        .acceleration = true,
        .palette = &palette,
        .width = width,
        .height = height,
    };
    auto drawOpaque = [&](bool accelerated) {
        frame.acceleration = accelerated;
        glBindFramebuffer(GL_FRAMEBUFFER, target);
        renderer.beginFrame(frame);
        for (int level = 0; level < LevelCount; ++level)
            if (uniforms[level]) renderer.drawLevel(*uniforms[level]);
        renderer.march();
    };

    // Parity: opaque colour/depth, all water channels and final composite, acceleration off vs on.
    struct Readback {
        std::vector<GLubyte> color, composite;
        std::vector<float> march;
    };
    auto readback = [&](bool accelerated) {
        Readback result;
        result.color.resize(pixels * 3);
        result.march.resize(pixels * 4);
        result.composite.resize(pixels * 4);
        drawOpaque(accelerated);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, result.color.data());
        glReadBuffer(GL_COLOR_ATTACHMENT1);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, result.march.data());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        if (opts.water) {
            renderer.composite();
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, result.composite.data());
        } else {
            for (std::size_t i = 0; i < pixels; ++i) {
                std::memcpy(&result.composite[i * 4], &result.color[i * 3], 3);
                result.composite[i * 4 + 3] = 255;
            }
        }
        glFinish();
        return result;
    };
    auto reference = readback(false);
    auto accelerated = readback(true);
    std::size_t colorDiffering = 0, distanceDiffering = 0, waterDiffering = 0, compositeDiffering = 0, nonBlack = 0;
    for (std::size_t i = 0; i < pixels; ++i) {
        if (std::memcmp(&reference.color[i * 3], &accelerated.color[i * 3], 3) != 0) ++colorDiffering;
        if (std::memcmp(&reference.march[i * 4], &accelerated.march[i * 4], sizeof(float)) != 0) ++distanceDiffering;
        if (std::memcmp(&reference.march[i * 4 + 1], &accelerated.march[i * 4 + 1], 3 * sizeof(float)) != 0) ++waterDiffering;
        if (std::memcmp(&reference.composite[i * 4], &accelerated.composite[i * 4], 4) != 0) ++compositeDiffering;
        if (accelerated.composite[i * 4] | accelerated.composite[i * 4 + 1] | accelerated.composite[i * 4 + 2]) ++nonBlack;
    }
    std::string const png = prefix + ".png";
    writePng(png, width, height, accelerated.composite);
    checkGL("mountains parity");
    require(colorDiffering == 0 && distanceDiffering == 0 && waterDiffering == 0 && compositeDiffering == 0,
            "Mountains acceleration changed opaque colour/depth, water intervals or composite");
    // Target visibility: hit pixels within 3 degrees of the view centre, plus the centre-row distance profile.
    constexpr float NoHit = 1e30f;
    auto const clipToView = glm::inverse(glm::perspective(glm::radians(65.0f), float(width) / float(height), 0.05f, 40000.0f));
    std::size_t coneHits = 0, conePixels = 0, rowHits = 0;
    float rowMin = NoHit, rowMax = 0, rowSum = 0, coneMin = NoHit, coneMax = 0;
    double const coneCos = std::cos(glm::radians(3.0));
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            glm::vec4 view = clipToView * glm::vec4((float(x) + 0.5f) / float(width) * 2 - 1, (float(y) + 0.5f) / float(height) * 2 - 1, 1, 1);
            glm::vec3 dir = glm::normalize(glm::vec3(view) / view.w);
            float d = accelerated.march[(std::size_t(y) * std::size_t(width) + std::size_t(x)) * 4];
            bool hit = d < NoHit;
            if (-dir.z >= coneCos) {
                ++conePixels;
                if (hit) { ++coneHits; coneMin = std::min(coneMin, d); coneMax = std::max(coneMax, d); }
            }
            if (y == height / 2 && hit) { ++rowHits; rowMin = std::min(rowMin, d); rowMax = std::max(rowMax, d); rowSum += d; }
        }

    GLuint queries[2];
    glGenQueries(2, queries);
    auto renderFrame = [&](bool accelerated) {
        glBindFramebuffer(GL_FRAMEBUFFER, target);
        glFinish();
        counters = {};
        auto start = Clock::now();
        glBeginQuery(GL_TIME_ELAPSED, queries[0]);
        drawOpaque(accelerated);
        glEndQuery(GL_TIME_ELAPSED);
        double opaqueSubmit = milliseconds(start);
        double compositeSubmit = 0;
        if (opts.water) {
            auto compositeStart = Clock::now();
            glBeginQuery(GL_TIME_ELAPSED, queries[1]);
            renderer.composite();
            glEndQuery(GL_TIME_ELAPSED);
            compositeSubmit = milliseconds(compositeStart);
        }
        glFinish();
        double wall = milliseconds(start);
        GLuint64 opaqueNs = 0, compositeNs = 0;
        glGetQueryObjectui64v(queries[0], GL_QUERY_RESULT, &opaqueNs);
        if (opts.water) glGetQueryObjectui64v(queries[1], GL_QUERY_RESULT, &compositeNs);
        require(counters.bytes == 0, "Mountains frame uploaded voxels");
        return FrameTimes{double(opaqueNs) / 1e6, double(compositeNs) / 1e6, opaqueSubmit, compositeSubmit, wall};
    };

    csv << "pair,frame,accelerated,opaque_gpu_ms,composite_gpu_ms,opaque_submit_ms,composite_submit_ms,cpu_submit_ms,wall_ms\n";
    std::ostringstream pairsJson;
    pairsJson << std::setprecision(9);
    std::array<std::vector<FrameTimes>, 2> overall;
    auto statsJson = [&](std::vector<FrameTimes> const& samples) {
        auto column = [&](auto&& pick) {
            std::vector<double> values;
            values.reserve(samples.size());
            for (auto const& s : samples) values.push_back(pick(s));
            return columnStats(std::move(values));
        };
        auto opaque = column([](FrameTimes const& t) { return t.opaqueGpu; });
        auto composite = column([](FrameTimes const& t) { return t.compositeGpu; });
        auto submit = column([](FrameTimes const& t) { return t.cpuSubmit(); });
        auto wall = column([](FrameTimes const& t) { return t.wall; });
        std::ostringstream s;
        s << std::setprecision(9)
          << "{\"frames\":" << samples.size()
          << ",\"opaque_gpu_ms\":{\"mean\":" << opaque.mean << ",\"p99\":" << opaque.p99 << "}"
          << ",\"composite_gpu_ms\":{\"mean\":" << composite.mean << ",\"p99\":" << composite.p99 << "}"
          << ",\"cpu_submit_ms\":{\"mean\":" << submit.mean << ",\"p99\":" << submit.p99 << "}"
          << ",\"wall_ms\":{\"mean\":" << wall.mean << ",\"p99\":" << wall.p99 << "}}";
        return s.str();
    };
    for (int pair = 0; pair < opts.pairs; ++pair) {
        std::array<std::vector<FrameTimes>, 2> samples;
        for (int index = -opts.warmup; index < opts.frames; ++index) {
            // Alternate order each paired frame to avoid a consistently warm path.
            for (int order = 0; order < 2; ++order) {
                int accelerated = (order + index + opts.warmup + pair) % 2;
                auto t = renderFrame(accelerated != 0);
                if (index < 0) continue;
                samples[accelerated].push_back(t);
                overall[accelerated].push_back(t);
                csv << pair << ',' << index << ',' << accelerated << ',' << t.opaqueGpu << ',' << t.compositeGpu << ','
                    << t.opaqueSubmit << ',' << t.compositeSubmit << ',' << t.cpuSubmit() << ',' << t.wall << '\n';
            }
        }
        for (int accelerated = 0; accelerated < 2; ++accelerated) {
            auto json = statsJson(samples[accelerated]);
            pairsJson << (pair == 0 && accelerated == 0 ? "" : ",")
                      << "{\"pair\":" << pair << ",\"accelerated\":" << accelerated << ",\"stats\":" << json << "}";
            log << "mountains,pose=" << poseName << ",pair=" << pair << ",accelerated=" << accelerated << ",stats=" << json << '\n';
        }
    }
    glDeleteQueries(2, queries);
    checkGL("mountains paired benchmark");

    int drawnTotal = 0, bricksTotal = 0;
    for (auto const& stats : levels) { drawnTotal += stats.drawn; bricksTotal += stats.bricks; }
    auto eyeMetres = metres(pose.eye), targetMetres = metres(pose.target);
    std::ofstream json(prefix + ".json");
    require(bool(json), "Cannot open mountains JSON");
    json << std::setprecision(9) << "{\n"
         << "  \"label\": \"" << opts.label << "\",\n"
         << "  \"scenario\": \"mountains\",\n"
         << "  \"pose\": \"" << poseName << "\",\n"
         << "  \"seed\": " << opts.seed << ",\n"
         << "  \"renderer\": \"" << glGetString(GL_RENDERER) << "\",\n"
         << "  \"version\": \"" << glGetString(GL_VERSION) << "\",\n"
         << "  \"width\": " << width << ", \"height\": " << height << ",\n"
         << "  \"frames\": " << opts.frames << ", \"warmup\": " << opts.warmup << ", \"pairs\": " << opts.pairs << ",\n"
         << "  \"water\": " << (opts.water ? "true" : "false") << ",\n"
         << "  \"render_mode\": \"" << (opts.water ? "voxel-water-composite" : "opaque-only-no-composite") << "\",\n"
         << "  \"eye_metres\": [" << eyeMetres.x << ", " << eyeMetres.y << ", " << eyeMetres.z << "],\n"
         << "  \"target_metres\": [" << targetMetres.x << ", " << targetMetres.y << ", " << targetMetres.z << "],\n"
         << "  \"anchor\": [" << anchor.x << ", " << anchor.y << ", " << anchor.z << "],\n"
         << "  \"cold_start\": {\"generate_upload_ms\": " << generateUploadMs << ", \"total_ms\": " << coldStartMs
         << ", \"targets\": " << targets.size() << ", \"uploaded_bytes\": " << uploadedBytes
         << ", \"gl_counted_texels\": " << coldCounters.bytes << ", \"subimage_calls\": " << coldCounters.subimage_calls
         << ", \"bricks_used\": " << renderer.bricks().used() << ", \"brick_capacity\": " << BrickCapacity << "},\n"
         << "  \"drawn_total\": " << drawnTotal << ", \"bricks_total\": " << bricksTotal << ",\n"
         << "  \"levels\": [\n";
    for (int level = 0; level < LevelCount; ++level) {
        auto const& s = levels[level];
        json << "    {\"level\": " << level << ", \"keys\": " << s.keys << ", \"bricks\": " << s.bricks
             << ", \"uniform_solid\": " << s.uniformSolid << ", \"uniform_air\": " << s.uniformAir
             << ", \"drawn\": " << s.drawn << ", \"drawn_bricks\": " << s.drawnBricks
             << ", \"drawn_solid\": " << s.drawnSolid << ", \"drawn_air\": " << s.drawnAir
             << ", \"hole\": " << (holeValid[level] ? "true" : "false") << "}" << (level + 1 < LevelCount ? ",\n" : "\n");
    }
    json << "  ],\n"
         << "  \"parity\": {\"color_differing\": " << colorDiffering << ", \"distance_differing\": " << distanceDiffering
         << ", \"water_differing\": " << waterDiffering << ", \"composite_differing\": " << compositeDiffering << ", \"pixels\": " << pixels << "},\n"
         << "  \"non_black_pixels\": " << nonBlack << ",\n"
         << "  \"centre_cone_3deg\": {\"pixels\": " << conePixels << ", \"hits\": " << coneHits
         << ", \"min_m\": " << (coneHits ? coneMin : 0) << ", \"max_m\": " << (coneHits ? coneMax : 0) << "},\n"
         << "  \"centre_row\": {\"pixels\": " << width << ", \"hits\": " << rowHits << ", \"min_m\": " << (rowHits ? rowMin : 0)
         << ", \"max_m\": " << (rowHits ? rowMax : 0) << ", \"mean_m\": " << (rowHits ? rowSum / float(rowHits) : 0) << "},\n"
         << "  \"png\": \"" << png << "\",\n"
         << "  \"pairs_stats\": [" << pairsJson.str() << "],\n"
         << "  \"overall\": {\"reference\": " << statsJson(overall[0]) << ", \"accelerated\": " << statsJson(overall[1]) << "}\n"
         << "}\n";
    log << "mountains,pose=" << poseName << ",eye=" << eyeMetres.x << '/' << eyeMetres.y << '/' << eyeMetres.z
        << ",targets=" << targets.size() << ",drawn=" << drawnTotal << ",bricks=" << bricksTotal
        << ",uploaded_bytes=" << uploadedBytes << ",generate_upload_ms=" << generateUploadMs << ",cold_start_ms=" << coldStartMs
        << ",color_differing=" << colorDiffering << ",distance_differing=" << distanceDiffering
        << ",water_differing=" << waterDiffering
        << ",composite_differing=" << compositeDiffering << ",non_black=" << nonBlack << '/' << pixels
        << ",cone_hits=" << coneHits << '/' << conePixels << ",row_hits=" << rowHits << '/' << width
        << ",png=" << png << '\n';
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteTextures(1, &targetColor);
    glDeleteFramebuffers(1, &target);
}

void benchmark(std::string const& scenario, Options const& opts, GLuint shader_program, std::ostream& csv, std::ostream& log) {
    const int count=scenario=="full" ? opts.dynamic_volumes : opts.volumes;
    const int side=int(std::ceil(std::sqrt(double(count))));
    const glm::vec3 center(side*0.5f,0.5f,side*0.5f), camera=center+glm::vec3(side*0.8f,side*0.7f,side*1.1f);
    const auto clip=glm::perspective(glm::radians(45.0f),1.0f,0.1f,1000.0f)*glm::lookAt(camera,center,glm::vec3(0,1,0));
    std::vector<std::unique_ptr<VoxelVolume>> volumes;
    for (int i=0;i<count;++i) {
        auto volume=std::make_unique<VoxelVolume>(128,128,128,Transform().translate(float(i%side),0,float(i/side)));
        volume->setProgram(shader_program);
        volume->setShaderSetting(fvta_step_material);
        // Continuous sinusoidal heightfield, not the original app's procedural world.
        volume->updateVoxels([i,side,&scenario](int x,int y,int z,GLubyte) {
            const auto height=48+24*std::sin((x+128*(i%side))*0.018)*std::cos((z+128*(i/side))*0.020);
            return GLubyte(scenario!="full" && y>=height ? 0 : 1+(x+3*y+7*z)%254);
        });
        volumes.push_back(std::move(volume));
    }
    // First allocation is recorded separately, not hidden inside measured frames.
    counters={}; glFinish(); auto initial=Clock::now();
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    for (auto& v:volumes) render(*v,clip,camera);
    glFinish(); checkGL("initial benchmark upload");
    log << scenario << ",initial_bytes=" << counters.bytes << ",initial_calls=" << counters.image_calls << ",initial_ms=" << milliseconds(initial) << '\n';
    for (int frame=-opts.warmup;frame<opts.frames;++frame) {
        const int tick=frame+opts.warmup;
        glFinish(); counters={}; auto start=Clock::now();
        for (auto& volume:volumes) {
            auto& v=*volume;
            if (scenario=="local") {
                v.setSphere(v.transform.apply(glm::vec3(0.0f,0.45f,0.5f)),0.025f,tick%2 ? 222 : 0);
            } else if (scenario=="scattered") {
                for(int i=0;i<8;++i) v.setVoxel((i&1)?127:0,(i&2)?127:0,i*18,GLubyte(1+tick%254));
            } else if (scenario=="full") {
                v.updateVoxels([tick](int x,int y,int z,GLubyte) { return GLubyte(1+(x+3*y+7*z+tick+1)%254); });
            }
        }
        auto mutation_ms=milliseconds(start);
        auto render_start=Clock::now();
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        for(auto& volume:volumes) render(*volume,clip,camera);
        glFinish();
        auto render_ms=milliseconds(render_start), frame_ms=milliseconds(start);
        checkGL("benchmark frame");
        if(opts.strict && scenario=="static") require(counters.bytes==0,"Static frame transferred voxels");
        if(frame>=0) csv << opts.label << ',' << scenario << ',' << count << ",128,128,128," << frame << ',' << counters.bytes << ',' << counters.image_calls << ',' << counters.subimage_calls << ',' << counters.upload_cpu_ms << ',' << counters.upload_drained_ms << ',' << mutation_ms << ',' << render_ms << ',' << frame_ms << ',' << (isolate_uploads?1:0) << '\n';
    }
    // Readbacks are outside timing and cover every volume's actual texture.
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    for(auto& volume:volumes) { render(*volume,clip,camera,true); log << scenario << ",texture_hash=" << texture_hash << '\n'; }
    glFinish(); checkGL("benchmark readback");
    log << scenario << ",image_hash=" << saveImage(opts.output+"-"+scenario+".rgba") << '\n';
}
}
int main(int argc,char** argv) {
    try {
        auto opts=options(argc,argv);
        bool const mountains = opts.scenario=="mountains";
        // Mountains accepts --output DIR; every legacy scenario keeps the prefix convention.
        std::string const prefix = mountains && std::filesystem::is_directory(opts.output)
            ? (std::filesystem::path(opts.output) / ("mountains-" + opts.pose)).string()
            : mountains ? opts.output + "-mountains-" + opts.pose : opts.output;
        std::ofstream csv(mountains ? prefix+"-frames.csv" : prefix+".csv"), log(prefix+".txt");
        require(bool(csv)&&bool(log),"Cannot open benchmark output prefix");
        csv << std::fixed << std::setprecision(6);
        if(!mountains) csv << "label,scenario,volumes,width,height,depth,frame,uploaded_bytes,image_calls,subimage_calls,upload_cpu_ms,upload_drained_ms,mutation_ms,render_finish_ms,whole_frame_ms,isolate_uploads\n";
        Surface surface;
        for(auto item:{GL_VENDOR,GL_RENDERER,GL_VERSION,GL_SHADING_LANGUAGE_VERSION}) log << item << '=' << glGetString(item) << '\n';
        log << "label=" << opts.label << ",warmup=" << opts.warmup << ",frames=" << opts.frames << ",framebuffer=" << framebuffer_pixels << 'x' << framebuffer_pixels << ",scenario=" << opts.scenario << ",isolate_uploads=" << isolate_uploads << '\n';
        atlasSmoke(log);
        if(mountains) {
            mountainsBenchmark(opts,prefix,csv,log);
            checkGL("shutdown");
            std::cout << "PASS " << prefix << " (.json, -frames.csv, .png, .txt)\n";
            return 0;
        }
        auto shader_program=program();
        smoke(shader_program,opts.strict,log,opts.output);
        raycastSmoke(opts.strict,log);
        if(opts.scenario!="smoke") for(auto const* scenario:{"static","local","scattered","full"}) if(opts.scenario=="all"||opts.scenario==scenario) benchmark(scenario,opts,shader_program,csv,log);
        glDeleteProgram(shader_program); checkGL("shutdown");
        std::cout << "PASS " << opts.output << " (.csv, .txt, .rgba)\n";
        return 0;
    } catch(std::exception const& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
