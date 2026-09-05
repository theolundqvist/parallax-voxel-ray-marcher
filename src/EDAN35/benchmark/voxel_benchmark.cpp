// Native instrumentation: every intercepted call is forwarded to the real GL driver.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
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

struct Capture {
    std::vector<GLubyte> color;
    std::vector<float> depth;
};
Capture capture(int pixels) {
    Capture result{std::vector<GLubyte>(pixels * pixels * 4),
                   std::vector<float>(pixels * pixels)};
    glReadPixels(0, 0, pixels, pixels, GL_RGBA, GL_UNSIGNED_BYTE, result.color.data());
    glReadPixels(0, 0, pixels, pixels, GL_DEPTH_COMPONENT, GL_FLOAT, result.depth.data());
    return result;
}

// Independent double-precision slab oracle over all occupied voxel boxes.
// Zero-area edge/corner contact is not a hit: tied DDA axes advance together.
void checkCenterRay(VoxelVolume const& volume, glm::vec3 camera, glm::vec3 target,
                    glm::mat4 const& clip, Capture const& image, int pixels) {
    glm::dvec3 origin(camera), direction = glm::normalize(glm::dvec3(target - camera));
    double nearest = 1e100;
    int material = 0;
    for (int z = 0; z < volume.D; ++z)
        for (int y = 0; y < volume.H; ++y)
            for (int x = 0; x < volume.W; ++x) {
                int value = volume.getVoxel(x, y, z);
                if (!value) continue;
                glm::dvec3 low = glm::dvec3(x,y,z) / glm::dvec3(volume.size());
                glm::dvec3 high = glm::dvec3(x+1,y+1,z+1) / glm::dvec3(volume.size());
                double enter = 0, exit = 1e100;
                for (int axis = 0; axis < 3; ++axis) {
                    if (direction[axis] == 0) {
                        if (origin[axis] < low[axis] || origin[axis] >= high[axis]) exit = -1;
                    } else {
                        double a = (low[axis] - origin[axis]) / direction[axis];
                        double b = (high[axis] - origin[axis]) / direction[axis];
                        enter = std::max(enter, std::min(a,b));
                        exit = std::min(exit, std::max(a,b));
                    }
                }
                if (enter < exit && enter < nearest) { nearest = enter; material = value; }
            }
    auto center = (pixels / 2) + pixels * (pixels / 2);
    require(image.color[center * 4] == (material ? material : 26),
            "World DDA center material differs from independent voxel-box oracle: actual=" +
            std::to_string(image.color[center*4]) + " expected=" + std::to_string(material) +
            " size=" + std::to_string(volume.W) + ":" + std::to_string(volume.H) + ":" + std::to_string(volume.D) +
            " camera=" + std::to_string(camera.x) + ":" + std::to_string(camera.y) + ":" + std::to_string(camera.z));
    float depth = 1;
    if (material) {
        auto position = glm::dvec4(origin + direction * nearest, 1);
        auto projected = glm::dmat4(clip) * position;
        depth = nearest == 0 ? 0 : float(std::clamp(projected.z / projected.w * 0.5 + 0.5, 0.0, 1.0));
    }
    require(std::isfinite(image.depth[center]) && std::abs(image.depth[center] - depth) < 0.00001f,
            "World DDA center depth differs from independent voxel-box oracle");
}

void worldSmoke(GLuint shader_program, std::ostream& log) {
    int pixels = std::min(framebuffer_pixels, 129);
    if (pixels % 2 == 0) --pixels;
    glViewport(0, 0, pixels, pixels);
    std::array<glm::vec3,256> palette;
    for (int i = 0; i < 256; ++i) palette[i] = glm::vec3((i % 7 + 1) / 8.0f, (i % 11 + 1) / 12.0f, (i % 13 + 1) / 14.0f);
    int comparisons = 0;
    for (auto size : {glm::ivec3(32), glm::ivec3(17,9,5), glm::ivec3(1)}) {
        VoxelVolume volume(size.x, size.y, size.z, Transform());
        volume.setProgram(shader_program);
        volume.setWorldStyle(true);
        volume.setPalette(palette);
        std::vector<uint8_t> data(std::size_t(size.x) * size.y * size.z);
        require(volume.empty(), "New volume occupancy is not empty");
        bool rejected = false;
        try { volume.setData(std::span<const uint8_t>(data).first(data.size()-1)); }
        catch (std::invalid_argument const&) { rejected = true; }
        require(rejected, "Mismatched world chunk dimensions accepted");
        for (int shape = 0; shape < 5; ++shape) {
            std::fill(data.begin(), data.end(), 0);
            for (int z=0; z<size.z; ++z) for (int y=0; y<size.y; ++y) for (int x=0; x<size.x; ++x) {
                bool solid = shape == 1 || (shape == 2 && (x == std::min(8,size.x-1) || z == size.z-1)) ||
                             (shape == 3 && x == size.x-1 && y == size.y/2 && z == size.z/2);
                if (solid) data[x + size.x * (y + size.y * z)] = uint8_t(1 + (x + y + z) % 5 + 16 * ((x + z) % 4));
            }
            volume.setData(data);
            if (shape == 4) {
                // Callback mutation must update zero transitions even when reentrant.
                volume.updateVoxels([&](int x,int y,int z,GLubyte) {
                    volume.setVoxel(x,y,z,5);
                    return GLubyte(0);
                });
            }
            require(volume.empty() == (shape == 0 || shape == 4), "World empty count differs from material data");
            counters = {};
            auto bytes = volume.upload();
            require(bytes == counters.bytes, "Explicit upload payload accounting mismatch");
            require(volume.upload() == 0, "Clean world upload transferred data");
            // Inspect the actual GL_R8 coarse texture after every mutation path.
            glActiveTexture(GL_TEXTURE1);
            auto cells = (size + 7) / 8;
            std::vector<GLubyte> coarse(std::size_t(cells.x) * cells.y * cells.z);
            GLint bound = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_3D, &bound);
            require(bound != 0, "Explicit world upload did not bind occupancy texture");
            glGetTexImage(GL_TEXTURE_3D, 0, GL_RED, GL_UNSIGNED_BYTE, coarse.data());
            for (int cz=0; cz<cells.z; ++cz) for (int cy=0; cy<cells.y; ++cy) for (int cx=0; cx<cells.x; ++cx) {
                bool occupied = false;
                for (int z=cz*8; z<std::min(cz*8+8,size.z); ++z)
                    for (int y=cy*8; y<std::min(cy*8+8,size.y); ++y)
                        for (int x=cx*8; x<std::min(cx*8+8,size.x); ++x)
                            occupied |= volume.getVoxel(x,y,z) != 0;
                require(coarse[cx + cells.x * (cy + cells.y * cz)] == (occupied ? 255 : 0),
                        "Coarse texture differs from actual voxel occupancy");
            }
            glActiveTexture(GL_TEXTURE0);
            // Axis rays pass through voxel interiors: raster interpolation can
            // perturb an ideal ray lying exactly along a material boundary.
            // Diagonal ties and every framebuffer pixel still require exact
            // reference/accelerated parity below.
            std::vector<glm::vec3> cameras = {{-1,.51f,.51f},{2,.51f,.51f},{.51f,-1,.51f},
                {.51f,2,.51f},{.51f,.51f,-1},{.51f,.51f,2},{-1,-1,-1},{.51f,.51f,.51f}};
            for (auto camera : cameras) {
                glm::vec3 target = camera == cameras.back() ? glm::vec3(1,.51f,.51f) : glm::vec3(.51f);
                auto direction = glm::normalize(target-camera);
                auto up = std::abs(direction.y) > .99f ? glm::vec3(0,0,1) : glm::vec3(0,1,0);
                auto clip = glm::perspective(glm::radians(50.0f), 1.0f, .001f, 20.0f) * glm::lookAt(camera,target,up);
                for (bool material : {true,false}) {
                    glUseProgram(shader_program);
                    glUniform1i(glGetUniformLocation(shader_program,"world_debug_material"),material);
                    glUniform1i(glGetUniformLocation(shader_program,"world_lighting"),true);
                    volume.setAcceleration(false);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    render(volume,clip,camera);
                    auto reference = capture(pixels);
                    if (material) checkCenterRay(volume,camera,target,clip,reference,pixels);
                    volume.setAcceleration(true);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    render(volume,clip,camera);
                    auto accelerated = capture(pixels);
                    require(reference.color == accelerated.color, "World accelerated color/material parity failed");
                    require(reference.depth == accelerated.depth, "World accelerated exact depth parity failed");
                    for (float depth : accelerated.depth) require(std::isfinite(depth), "World depth is not finite");
                    ++comparisons;
                }
            }
        }
    }
    glUseProgram(shader_program);
    glUniform1i(glGetUniformLocation(shader_program,"world_debug_material"),false);
    glUseProgram(0);
    glViewport(0,0,framebuffer_pixels,framebuffer_pixels);
    checkGL("world reference/accelerated smoke");
    log << "world,parity_pairs=" << comparisons << ",material-and-lit-color=byte-exact,depth=bit-exact,center-slab-oracle=pass,occupancy-readback=pass\n";
}

void skySmoke(GLuint voxel_program, std::ostream& log) {
    auto sky_program = program("worldsky");
    GLuint vao;
    glGenVertexArrays(1,&vao);
    glm::vec3 camera(-1,.5f,.5f);
    auto clip = glm::perspective(glm::radians(50.0f),1.0f,.001f,20.0f) *
        glm::lookAt(camera,glm::vec3(.5f),glm::vec3(0,1,0));
    auto inverse = glm::inverse(clip);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUseProgram(sky_program);
    glUniformMatrix4fv(glGetUniformLocation(sky_program,"clip_to_world"),1,GL_FALSE,glm::value_ptr(inverse));
    glUniform3fv(glGetUniformLocation(sky_program,"camera_position"),1,glm::value_ptr(camera));
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES,0,3);
    auto sky = capture(framebuffer_pixels);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    VoxelVolume volume(1,1,1,Transform());
    volume.setProgram(voxel_program);
    volume.setWorldStyle(true);
    volume.setVoxel(0,0,0,5);
    glUseProgram(voxel_program);
    glUniform1i(glGetUniformLocation(voxel_program,"world_lighting"),true);
    glUniform1i(glGetUniformLocation(voxel_program,"world_debug_material"),false);
    glUniform1f(glGetUniformLocation(voxel_program,"world_fog_radius"),.01f);
    render(volume,clip,camera);
    auto fogged = capture(framebuffer_pixels);
    for (std::size_t i=0; i<sky.color.size(); ++i)
        require(std::abs(int(sky.color[i])-int(fogged.color[i])) <= 1,
                "Fully fogged geometry does not converge to actual fullscreen sky");
    int center = framebuffer_pixels/2 + framebuffer_pixels*(framebuffer_pixels/2);
    require(fogged.depth[center] < 1, "Sky/fog parity scene did not draw geometry");
    glUseProgram(voxel_program);
    glUniform1f(glGetUniformLocation(voxel_program,"world_fog_radius"),world::FogDistance);
    glUseProgram(0);
    glDeleteVertexArrays(1,&vao);
    glDeleteProgram(sky_program);
    checkGL("fullscreen sky and fog smoke");
    log << "world,fullscreen-sky-and-fog-convergence=pass\n";
}

struct Options {
    int volumes=100, dynamic_volumes=4, frames=100, warmup=10;
    int pairs=1;
    bool world_lighting=true;
    std::uint64_t seed = world::DefaultSeed;
    bool strict=false;
    std::string pose = "spawn";
    std::string label="candidate", scenario="all", output="voxel-benchmark";
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
        else if (arg=="--world-lighting") result.world_lighting=std::stoi(value)!=0;
        else if (arg=="--pose") result.pose=value;
        else if (arg=="--seed") result.seed=std::stoull(value);
        else throw std::runtime_error("Unknown option "+arg);
    }
    require(result.volumes>0 && result.dynamic_volumes>0 && result.frames>0 && result.warmup>=0,"Invalid workload sizes");
    require(framebuffer_pixels>0 && framebuffer_pixels<=4096,"Invalid framebuffer size");
    require(result.pose=="spawn" || result.pose=="cave" || result.pose=="underside","Invalid world pose");
    require(result.pairs>0,"Invalid pair count");
    require(result.scenario=="all" || result.scenario=="islands" || result.scenario=="world" || result.scenario=="smoke" || result.scenario=="static" || result.scenario=="local" || result.scenario=="scattered" || result.scenario=="full","Invalid scenario");
    return result;
}
void worldBenchmark(Options const& opts, GLuint shader_program, std::ostream& log) {
    std::ofstream csv(opts.output + "-world.csv");
    require(bool(csv), "Cannot open world benchmark CSV");
    csv << "pair,frame,accelerated,lighting,volumes,gpu_ms,cpu_submit_ms,cpu_finish_ms,uploaded_bytes\n";
    bool const islands = opts.scenario == "islands";
    const int side = int(std::ceil(std::sqrt(double(opts.volumes))));
    glm::vec3 center(side * .5f,.5f,side * .5f);
    glm::vec3 camera = center + glm::vec3(side * .8f,side * .7f,side * 1.1f);
    auto clip = glm::perspective(glm::radians(45.0f),1.0f,.01f,1000.0f) *
                glm::lookAt(camera,center,glm::vec3(0,1,0));
    if (islands) {
        camera = world::spawnPosition(opts.seed);
        center = world::spawnTarget(opts.seed);
        if (opts.pose == "cave") { camera = glm::vec3(0,1.5f,19.5f); center = glm::vec3(0,.5f,14.7f); }
        else if (opts.pose == "underside") { camera = glm::vec3(0,-12,26); center = glm::vec3(0,-6,0); }
        clip = glm::perspective(glm::radians(65.0f),1.0f,.01f,1000.0f) *
            glm::lookAt(camera,center,glm::vec3(0,1,0));
    }
    std::vector<std::unique_ptr<VoxelVolume>> volumes;
    std::array<glm::vec3,256> palette;
    for (int i=0; i<256; ++i) palette[i] = glm::vec3(.3f,.5f,.2f) * (.7f + .02f * (i >> 4));
    counters = {};
    auto generation_start = Clock::now();
    if (islands) {
        palette = world::worldPalette();
        auto camera_key = world::keyAt(camera);
        constexpr int radius = world::LoadRadius;
        for (int z=-radius; z<=radius; ++z) for (int y=-radius; y<=radius; ++y) for (int x=-radius; x<=radius; ++x) {
            if (x*x+y*y+z*z > radius*radius) continue;
            world::ChunkKey key{camera_key.x+x,camera_key.y+y,camera_key.z+z};
            if (!world::valid(key)) continue;
            auto data = world::generateChunk(opts.seed,key);
            if (std::none_of(data.begin(),data.end(),[](auto material) { return material != 0; })) continue;
            auto position = world::origin(key);
            auto volume = std::make_unique<VoxelVolume>(32,32,32,
                Transform().translate(position.x,position.y,position.z).scale(world::ChunkSpan));
            volume->setProgram(shader_program);
            volume->setWorldStyle(true);
            volume->setData(data);
            volume->upload();
            volumes.push_back(std::move(volume));
        }
    } else for (int i=0; i<opts.volumes; ++i) {
        auto volume = std::make_unique<VoxelVolume>(32,32,32,
            Transform().translate(float(i%side),0,float(i/side)));
        volume->setProgram(shader_program);
        volume->setWorldStyle(true);
        volume->updateVoxels([i](int x,int y,int z,GLubyte) {
            float dx=x-15.5f, dz=z-15.5f;
            bool solid = dx*dx+dz*dz < 210 && y < 12 + 3*std::sin((x+z+i)*.2f) && y > 3;
            bool cave = (y-8)*(y-8)+(z-16)*(z-16) < 10;
            return GLubyte(solid && !cave ? 1 + (x+z)%5 + 16*((x+y+z)%4) : 0);
        });
        volume->upload();
        volumes.push_back(std::move(volume));
    }
    log << "world,initial_bytes=" << counters.bytes << ",initial_texture_allocations=" << counters.image_calls
        << ",workload=" << (islands ? "generated-islands" : "32-cubed-cave-islands")
        << ",load_radius=" << (islands ? world::LoadRadius : 0)
        << ",pose=" << opts.pose
        << ",seed=" << opts.seed << ",volumes=" << volumes.size()
        << ",generation-and-upload_ms=" << milliseconds(generation_start)
        << ",lighting=" << opts.world_lighting << '\n';
    glUseProgram(shader_program);
    glUniform3fv(glGetUniformLocation(shader_program,"colorPalette"),256,glm::value_ptr(palette[0]));
    glUniform1i(glGetUniformLocation(shader_program,"world_lighting"),opts.world_lighting);
    glUniform1i(glGetUniformLocation(shader_program,"world_debug_material"),false);
    glUniform1f(glGetUniformLocation(shader_program,"world_fog_radius"),islands ? world::FogDistance : 1000.0f);
    GLuint sky_program = 0, sky_vao = 0;
    if (islands) {
        sky_program = program("worldsky");
        glGenVertexArrays(1,&sky_vao);
        glUseProgram(sky_program);
        auto inverse = glm::inverse(clip);
        glUniformMatrix4fv(glGetUniformLocation(sky_program,"clip_to_world"),1,GL_FALSE,glm::value_ptr(inverse));
        glUniform3fv(glGetUniformLocation(sky_program,"camera_position"),1,glm::value_ptr(camera));
    }
    auto draw_scene = [&](bool verify = false, bool sky = true) {
        if (sky_program && sky) {
            glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
            glUseProgram(sky_program); glBindVertexArray(sky_vao);
            glDrawArrays(GL_TRIANGLES,0,3);
            glBindVertexArray(0); glDepthMask(GL_TRUE); glEnable(GL_DEPTH_TEST);
        }
        for (auto& volume : volumes) render(*volume,clip,camera,verify);
    };
    // Whole generated scene parity is proven before timing, with material bytes
    // and actual hit depths as well as identical controllable lighting.
    for (bool material : {true,false}) {
        glUseProgram(shader_program);
        glUniform1i(glGetUniformLocation(shader_program,"world_debug_material"),material);
        Capture reference;
        for (bool accelerated : {false,true}) {
            for (auto& volume : volumes) volume->setAcceleration(accelerated);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            draw_scene(false,!material);
            auto frame = capture(framebuffer_pixels);
            auto suffix = std::string(material ? "-material" : "-shaded") + (accelerated ? "-accelerated" : "-reference");
            saveImage(opts.output + suffix + ".rgba");
            std::ofstream depth_file(opts.output + suffix + ".depth",std::ios::binary);
            require(bool(depth_file),"Cannot write world depth capture");
            depth_file.write(reinterpret_cast<char const*>(frame.depth.data()),std::streamsize(frame.depth.size()*sizeof(float)));
            if (!accelerated) reference = std::move(frame);
            else {
                require(reference.color == frame.color,"Whole-world material/shaded parity failed");
                require(reference.depth == frame.depth,"Whole-world exact depth parity failed");
            }
        }
    }
    log << "world,whole-scene-material-and-shaded-parity=byte-exact,depth=bit-exact\n";
    GLuint timer;
    glGenQueries(1,&timer);
    for (int pair=0; pair<opts.pairs; ++pair) {
        std::array<std::vector<double>,2> gpu_times, cpu_times;
        for (int frame=-opts.warmup; frame<opts.frames; ++frame) {
            // Alternate order each paired frame to avoid a consistently warm path.
            for (int order=0; order<2; ++order) {
                int accelerated = (order + frame + opts.warmup + pair) % 2;
                for (auto& volume : volumes) volume->setAcceleration(accelerated != 0);
                glFinish();
                counters = {};
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                auto start = Clock::now();
                glBeginQuery(GL_TIME_ELAPSED,timer);
                draw_scene();
                glEndQuery(GL_TIME_ELAPSED);
                double submit_ms = milliseconds(start);
                glFinish();
                double finish_ms = milliseconds(start);
                GLuint64 nanoseconds = 0;
                glGetQueryObjectui64v(timer,GL_QUERY_RESULT,&nanoseconds);
                double gpu_ms = double(nanoseconds) / 1e6;
                require(counters.bytes == 0, "Unchanged world benchmark uploaded voxels");
                if (frame>=0) {
                    gpu_times[accelerated].push_back(gpu_ms);
                    cpu_times[accelerated].push_back(finish_ms);
                    csv << pair << ',' << frame << ',' << accelerated << ',' << opts.world_lighting << ','
                        << volumes.size() << ',' << gpu_ms << ',' << submit_ms << ',' << finish_ms << ',' << counters.bytes << '\n';
                }
            }
        }
        for (int accelerated=0; accelerated<2; ++accelerated) {
            auto& gpu = gpu_times[accelerated];
            auto& cpu = cpu_times[accelerated];
            std::sort(gpu.begin(),gpu.end()); std::sort(cpu.begin(),cpu.end());
            auto percentile = [](auto const& samples, double p) { return samples[std::size_t(std::ceil(samples.size()*p))-1]; };
            log << "world,pair=" << pair << ",accelerated=" << accelerated
                << ",gpu_p50_ms=" << percentile(gpu,.5) << ",gpu_p95_ms=" << percentile(gpu,.95)
                << ",cpu_finish_p50_ms=" << percentile(cpu,.5) << ",cpu_finish_p95_ms=" << percentile(cpu,.95) << '\n';
        }
    }
    glDeleteQueries(1,&timer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw_scene(true);
    log << "world,image_hash=" << saveImage(opts.output + "-world.rgba") << '\n';
    checkGL("world paired benchmark");
    if (sky_program) { glDeleteVertexArrays(1,&sky_vao); glDeleteProgram(sky_program); }
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
        std::ofstream csv(opts.output+".csv"), log(opts.output+".txt");
        require(bool(csv)&&bool(log),"Cannot open benchmark output prefix");
        csv << std::fixed << std::setprecision(6);
        csv << "label,scenario,volumes,width,height,depth,frame,uploaded_bytes,image_calls,subimage_calls,upload_cpu_ms,upload_drained_ms,mutation_ms,render_finish_ms,whole_frame_ms,isolate_uploads\n";
        Surface surface;
        for(auto item:{GL_VENDOR,GL_RENDERER,GL_VERSION,GL_SHADING_LANGUAGE_VERSION}) log << item << '=' << glGetString(item) << '\n';
        log << "label=" << opts.label << ",warmup=" << opts.warmup << ",frames=" << opts.frames << ",framebuffer=" << framebuffer_pixels << 'x' << framebuffer_pixels << ",scenario=" << opts.scenario << ",isolate_uploads=" << isolate_uploads << '\n';
        auto shader_program=program();
        smoke(shader_program,opts.strict,log,opts.output);
        raycastSmoke(opts.strict,log);
        worldSmoke(shader_program,log);
        skySmoke(shader_program,log);
        if(opts.scenario!="smoke") for(auto const* scenario:{"static","local","scattered","full"}) if(opts.scenario=="all"||opts.scenario==scenario) benchmark(scenario,opts,shader_program,csv,log);
        if(opts.scenario=="all" || opts.scenario=="world" || opts.scenario=="islands") worldBenchmark(opts,shader_program,log);
        glDeleteProgram(shader_program); checkGL("shutdown");
        std::cout << "PASS " << opts.output << " (.csv, .txt, .rgba)\n";
        return 0;
    } catch(std::exception const& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
