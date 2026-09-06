#include "WorldRenderer.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>

namespace world {
namespace {
constexpr GLuint LevelBinding = 0;

GLuint texture2D(GLenum internalFormat, int width, int height, GLenum format, GLenum type) {
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GLint(internalFormat), width, height, 0, format, type, nullptr);
    return id;
}
}

WorldRenderer::~WorldRenderer() {
    glDeleteVertexArrays(1, &emptyVao);
    glDeleteBuffers(1, &levelBuffer);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &colorTexture);
    glDeleteTextures(1, &distanceTexture);
}

void WorldRenderer::init(ShaderProgramManager& programs) {
    programs.CreateAndRegisterProgram("world-voxel",
        {{ShaderType::vertex, "EDAN35/fullscreen.vert"}, {ShaderType::fragment, "EDAN35/world.frag"}}, worldProgram);
    programs.CreateAndRegisterProgram("world-composite",
        {{ShaderType::vertex, "EDAN35/fullscreen.vert"}, {ShaderType::fragment, "EDAN35/composite.frag"}},
        compositeProgram);
    if (worldProgram == 0 || compositeProgram == 0)
        throw std::runtime_error("World shader compilation failed; see shader log");
    refreshPrograms();
    pool.init();
    atlas.init();
    for (int level = 0; level < LevelCount; ++level) tables[level].init(atlas, level);
    glGenBuffers(1, &levelBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, levelBuffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(levels), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glGenVertexArrays(1, &emptyVao);
}

void WorldRenderer::refreshPrograms() {
    auto at = [](GLuint program, char const* name) { return glGetUniformLocation(program, name); };
    world = {at(worldProgram, "volume"), at(worldProgram, "coarse_occupancy"), at(worldProgram, "page_tables"),
             at(worldProgram, "world_acceleration"), at(worldProgram, "clip_to_world"),
             at(worldProgram, "camera_position"), at(worldProgram, "sun_direction"), at(worldProgram, "colorPalette"),
             glGetUniformBlockIndex(worldProgram, "Levels")};
    glUniformBlockBinding(worldProgram, world.levels, LevelBinding);
    comp = {at(compositeProgram, "color_buffer"), at(compositeProgram, "distance_buffer"),
            at(compositeProgram, "clip_to_world"), at(compositeProgram, "camera_position"),
            at(compositeProgram, "sun_direction")};
}

void WorldRenderer::resize(int w, int h) {
    if (w == width && h == height && framebuffer != 0) return;
    width = w;
    height = h;
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &colorTexture);
    glDeleteTextures(1, &distanceTexture);
    colorTexture = texture2D(GL_RGB8, w, h, GL_RGB, GL_UNSIGNED_BYTE);
    distanceTexture = texture2D(GL_RGBA32F, w, h, GL_RGBA, GL_FLOAT);
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, distanceTexture, 0);
    GLenum const attachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, attachments);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("World framebuffer is incomplete");
    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(targetFramebuffer));
}

void WorldRenderer::beginFrame(FrameUniforms const& frame) {
    current = frame;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &targetFramebuffer);
    resize(frame.width, frame.height);
    for (auto& level : levels) level.sizeDrawn.w = 0;
}

void WorldRenderer::drawLevel(LevelUniforms const& level) {
    levels[level.level] = {glm::vec4(level.regionOrigin, level.chunkSpan), glm::ivec4(level.regionSize, level.topRow >= 0 ? 1 : 0),
                           glm::ivec4(level.pageOrigin, level.topRow), glm::ivec4(level.holeLo, 0), glm::ivec4(level.holeHi, 0)};
}

void WorldRenderer::march() {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(emptyVao);

    glBindBuffer(GL_UNIFORM_BUFFER, levelBuffer);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(levels), levels.data());
    glBindBufferBase(GL_UNIFORM_BUFFER, LevelBinding, levelBuffer);
    glUseProgram(worldProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, pool.texture());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, pool.occupancyTexture());
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, atlas.texture());
    glUniform1i(world.volume, 0);
    glUniform1i(world.occupancy, 1);
    glUniform1i(world.pageTables, 2);
    glUniform1i(world.acceleration, current.acceleration);
    glUniformMatrix4fv(world.clipToWorld, 1, GL_FALSE, glm::value_ptr(current.clipToWorld));
    glUniform3fv(world.camera, 1, glm::value_ptr(current.cameraPosition));
    glUniform3fv(world.sun, 1, glm::value_ptr(current.sunDirection));
    glUniform3fv(world.palette, 256, glm::value_ptr((*current.palette)[0]));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindTexture(GL_TEXTURE_3D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, 0);
    glBindVertexArray(0);
}

void WorldRenderer::composite() {
    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(targetFramebuffer));
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glBindVertexArray(emptyVao);
    glUseProgram(compositeProgram);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, distanceTexture);
    glUniform1i(comp.color, 0);
    glUniform1i(comp.distance, 1);
    glUniformMatrix4fv(comp.clipToWorld, 1, GL_FALSE, glm::value_ptr(current.clipToWorld));
    glUniform3fv(comp.camera, 1, glm::value_ptr(current.cameraPosition));
    glUniform3fv(comp.sun, 1, glm::value_ptr(current.sunDirection));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}
}
