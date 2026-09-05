// Native instrumentation: every intercepted call is forwarded to the real GL driver.
#include <algorithm>
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
        glGetIntegerv(GL_TEXTURE_BINDING_3D, &bound);
        sampled_texture = GLuint(bound);
        std::vector<GLubyte> data(expected->size());
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glGetTexImage(GL_TEXTURE_3D, 0, GL_RED, GL_UNSIGNED_BYTE, data.data());
        require(data == *expected, "Real 3D texture readback differs from CPU voxels");
        texture_hash = hash(data);
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
GLuint program() {
    auto vs = shader(GL_VERTEX_SHADER, std::string(VOXEL_BENCHMARK_SHADER_DIR) + "/voxel.vert");
    auto fs = shader(GL_FRAGMENT_SHADER, std::string(VOXEL_BENCHMARK_SHADER_DIR) + "/voxel.frag");
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
    if (strict) require(counters.image_calls == 1 && counters.bytes == 17 * 9 * 5, "Initial upload not exactly once");
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
        require(glIsTexture(texture)==GL_TRUE,"Volume texture not alive after render");
        volume.reset();
        require(glIsTexture(texture)==GL_FALSE,"Volume texture not deleted by destructor");
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
        else throw std::runtime_error("Unknown option "+arg);
    }
    require(result.volumes>0 && result.dynamic_volumes>0 && result.frames>0 && result.warmup>=0,"Invalid workload sizes");
    require(framebuffer_pixels>0 && framebuffer_pixels<=4096,"Invalid framebuffer size");
    require(result.scenario=="all" || result.scenario=="smoke" || result.scenario=="static" || result.scenario=="local" || result.scenario=="scattered" || result.scenario=="full","Invalid scenario");
    return result;
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
        log << "label=" << opts.label << ",warmup=" << opts.warmup << ",frames=" << opts.frames << ",framebuffer=" << framebuffer_pixels << 'x' << framebuffer_pixels << ",shader=fvta_step_material,scene=sinusoidal-heightfield(full:solid),isolate_uploads=" << isolate_uploads << '\n';
        auto shader_program=program();
        smoke(shader_program,opts.strict,log,opts.output);
        raycastSmoke(opts.strict,log);
        if(opts.scenario!="smoke") for(auto const* scenario:{"static","local","scattered","full"}) if(opts.scenario=="all"||opts.scenario==scenario) benchmark(scenario,opts,shader_program,csv,log);
        glDeleteProgram(shader_program); checkGL("shutdown");
        std::cout << "PASS " << opts.output << " (.csv, .txt, .rgba)\n";
        return 0;
    } catch(std::exception const& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
