#pragma once

#include "EDAN35/util/Transform.cpp"
#include "core/helpers.hpp"
#include "core/opengl.hpp"
#include <glm/gtx/component_wise.hpp>
#include "../util/colorPalette.hpp"
#include <algorithm>
#include <vector>
#include <span>
#include <cstdint>
#include <stdexcept>


class VoxelVolume {
private:
    GLubyte *texels;
    GLuint texture{};
    GLuint occupancy_texture{};
    glm::ivec3 coarse_size{};
    std::vector<uint16_t> occupancy_counts;
    std::vector<GLubyte> occupancy;
    std::vector<bool> occupancy_dirty;
    std::size_t solid_count = 0;
    bool acceleration = true;
    bool world_style = false;
    bool coarse_dirty = false;
    struct DirtySlice {
        int min_x, min_y, max_x, max_y;
        bool operator==(DirtySlice const &) const = default;
    };
    std::vector<DirtySlice> dirty_slices;
    bool dirty = false;
    bonobo::mesh_data bounding_box;
    IntersectionTests::box_t local_space_AABB = {.min=glm::vec3(0.0), .max=glm::vec3(1.0)};
    static std::vector<glm::vec3> const &defaultPalette() {
        static auto const palette = [] {
            auto colors = colorPalette::CAColorsBlue2Pink;
            return colorPalette::generateCAColorPalette(colors, glm::ivec2(0, 255));
        }();
        return palette;
    }
    std::vector<glm::vec3> colorPalette;

    GLuint program{};
    shader_setting_t shader_setting = fixed_step_material;
    struct Uniforms {
        GLint volume, occupancy, shader, acceleration, voxel_size, lod, grid_size;
        GLint model_to_world, world_to_model, normal_to_world, world_to_clip;
        GLint camera, light, palette;
    } uniforms{};

public:
    Transform transform;
    float voxel_size = 0.1f;
    int LOD = 1;
    const int W;
    const int H;
    const int D;

    VoxelVolume(const int WIDTH, const int HEIGHT, const int DEPTH, Transform tf)
        : W(WIDTH), H(HEIGHT), D(DEPTH) {
        this->transform = tf;
        if (W <= 0 || H <= 0 || D <= 0)
            throw std::invalid_argument("VoxelVolume dimensions must be positive");
        coarse_size = (glm::ivec3(W, H, D) + 7) / 8;
        auto cells = std::size_t(coarse_size.x) * coarse_size.y * coarse_size.z;
        occupancy_counts.resize(cells);
        occupancy.resize(cells);
        occupancy_dirty.resize(cells);
        dirty_slices.resize(D, {W, H, 0, 0});

        voxel_size = 1.0f / (float) W;

        texels = (GLubyte *) calloc(W * H * D, sizeof(GLubyte));
        //bounding_box = parametric_shapes::createQuad(1.0f, 1.0f, 0, 0);
        bounding_box = parametric_shapes::createCube(1.0f, 1.0f, 1.0f);
    }

    VoxelVolume(VoxelVolume const &) = delete;
    VoxelVolume &operator=(VoxelVolume const &) = delete;

    void setLOD(int lod) {
        LOD = lod;
    }

    ~VoxelVolume() {
        glDeleteTextures(1, &texture);
        glDeleteTextures(1, &occupancy_texture);
        glDeleteVertexArrays(1, &bounding_box.vao);
        glDeleteBuffers(1, &bounding_box.bo);
        glDeleteBuffers(1, &bounding_box.ibo);
        free(texels);
    }

    glm::ivec3 size() const {
        return {W, H, D};
    }

    glm::vec3 sizef() const {
        return {W, H, D};
    }


    void setProgram(GLuint shaderProgram) {
        program = shaderProgram;
        auto location = [&](char const *name) { return glGetUniformLocation(program, name); };
        uniforms = {location("volume"), location("coarse_occupancy"),
            location("Shader_manager"), location("world_acceleration"),
            location("voxel_size"), location("lod"), location("grid_size"),
            location("model_to_world"), location("world_to_model"),
            location("normal_model_to_world"), location("vertex_world_to_clip"),
            location("camera_position"), location("light_direction"), location("colorPalette")};
    }

    bool empty() const { return solid_count == 0; }
    void setAcceleration(bool enabled) { acceleration = enabled; }
    void setWorldStyle(bool enabled) { world_style = enabled; }

    void setPalette(std::span<const glm::vec3> palette) {
        if (palette.size() != 256)
            throw std::invalid_argument("VoxelVolume palette must contain 256 colors");
        colorPalette.assign(palette.begin(), palette.end());
    }

    void setData(std::span<const uint8_t> data) {
        if (data.size() != std::size_t(W) * H * D)
            throw std::invalid_argument("VoxelVolume data size does not match dimensions");
        for (int z = 0; z < D; ++z)
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    setVoxel(x, y, z, data[x + W * (y + H * z)]);
    }

    std::size_t upload() {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, texture);
        auto bytes = uploadTexture();
        bytes += uploadOccupancy();
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_3D, occupancy_texture);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, texture);
        return bytes;
    }

    int getVoxel(int x, int y, int z) const {
        if (x < 0 || x >= W || y < 0 || y >= H || z < 0 || z >= D) return -1;
        return texels[x + y * W + z * W * H];
    }

    int getVoxel(glm::ivec3 index) const {
        return getVoxel(index.x, index.y, index.z);
    }

    bool setVoxel(int x, int y, int z, GLubyte value) {
        if (x < 0 || x >= W || y < 0 || y >= H || z < 0 || z >= D) return false;
        auto i = x + y * W + z * W * H;
        if (texels[i] != value) {
            if ((texels[i] == 0) != (value == 0)) {
                auto cell = x / 8 + coarse_size.x * (y / 8 + coarse_size.y * (z / 8));
                auto &count = occupancy_counts[cell];
                bool was_empty = count == 0;
                if (value != 0) { ++count; ++solid_count; }
                else { --count; --solid_count; }
                if (was_empty != (count == 0)) {
                    occupancy[cell] = count == 0 ? 0 : 255;
                    occupancy_dirty[cell] = true;
                    coarse_dirty = true;
                }
            }
            texels[i] = value;
            markDirty(x, y, z);
        }
        return true;
    }

    bool setVoxel(glm::ivec3 index, GLubyte value) {
        return setVoxel(index.x, index.y, index.z, value);
    }

    void cleanVoxel() {
        for (int x = 0; x < W; x++) {
            for (int y = 0; y < H; y++) {
                for (int z = 0; z < D; z++) {
                    setVoxel(x, y, z, 0);
                }
            }
        }
    }

    void generateColorPalette(std::vector<glm::vec3> colors, glm::vec2 colorRange) {
        this->colorPalette = colorPalette::generateCAColorPalette(colors,colorRange);
    }

    void updateVoxels(std::function<GLubyte(int x,int y,int z, GLubyte previous)> const get_material){
        for (int z = 0; z < D; z++) {
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    auto i = x + y * W + z * W * H;
                    auto value = get_material(x, y, z, texels[i]);
                    setVoxel(x, y, z, value);
                }
            }
        }
    }

    void setVolumeData3D(GLubyte ***data) {
        for (int x = 0; x < W; x++) {
            for (int y = 0; y < H; y++) {
                for (int z = 0; z < D; z++) {
                    setVoxel(x, y, z, data[x][y][z]);
                }
            }
        }
    }

    typedef struct voxel_hit_t {
        bool miss;
        glm::ivec3 index;
        GLubyte material;
        VoxelVolume *volume;
        glm::vec3 world_pos;
    } voxel_hit_t;

    voxel_hit_t isInside(glm::vec3 world_pos) {
        auto local_pos = transform.get_inverse().apply(world_pos);
        if (!IntersectionTests::PointInBox(local_pos, local_space_AABB))
            return {.miss=true};

        auto index = glm::min(localToIndex(local_pos), size() - 1);
        return {
                .miss=false,
                .index=index,
                .material=(GLubyte)getVoxel(index),
                .volume=this,
                .world_pos=world_pos
        };
    }

    bool intersectsSphere(glm::vec3 center, float radius) {
        auto inverse = transform.get_inverse();
        auto local_center = inverse.apply(center);
        auto local_radius = radius / transform.getScale().x;

        return IntersectionTests::BoxIntersectsSphere(
                local_space_AABB,
                local_center,
                local_radius
        );
    }

    void setSphere(glm::vec3 center, float radius, int material) {

        auto inverse = transform.get_inverse();
        auto local_center = inverse.apply(center);
        auto local_radius = radius / transform.getScale().x;  // will have to do
        auto min = local_center - glm::vec3(local_radius);
        auto max = local_center + glm::vec3(local_radius);
        auto index_center = localToIndex(local_center);
        auto min_index = localToIndex(min);
        auto max_index = localToIndex(max);
        auto index_radius = local_radius * this->W;
        float r2 = index_radius * index_radius;
        min_index.x = std::max(min_index.x, 0);
        min_index.y = std::max(min_index.y, 0);
        min_index.z = std::max(min_index.z, 0);
        max_index.x = std::min(max_index.x, W - 1);
        max_index.y = std::min(max_index.y, H - 1);
        max_index.z = std::min(max_index.z, D - 1);
        for (int x = min_index.x; x <= max_index.x; x++) {
            for (int y = min_index.y; y <= max_index.y; y++) {
                for (int z = min_index.z; z <= max_index.z; z++) {
                    auto index = glm::ivec3(x, y, z);
                    if (glm::length2(glm::vec3(index - index_center)) <= r2) {
                        auto mat = material == -1 ? voxel_util::hash(index) : material;
                        setVoxel(index, mat);
                    }
                }
            }
        }
    }


    voxel_hit_t raycast(glm::vec3 w_origin, glm::vec3 w_direction) {
        //check if bounding box is hit
        auto inverse = transform.get_inverse();
        auto dir = inverse.applyRotation(w_direction);
        auto local_origin = inverse.apply(w_origin);
        auto hit = IntersectionTests::RayIntersectsBox(
                local_space_AABB,
                {
                        local_origin,
                        dir,
                        glm::vec3(1.0f) / dir
                });
        if (!hit.miss) {
            if (glm::length2(hit.near - hit.far) > glm::length2(local_origin - hit.far)) {
                //camera inside BB
                hit.near = local_origin;
            }
            auto local_pos = glm::clamp(hit.near, local_space_AABB.min, local_space_AABB.max);
            float step_size = 1.0 / 10;
            auto step = normalize(w_direction * transform.getScale()) * voxel_size * step_size;
            auto local_step = inverse.applyRotation(step);
            int i = 0;
            int max_step = (int) (1.0 / step_size * 1.0 / step_size * 1.0 / voxel_size) * 30;
            for (i = 0; i < max_step; ++i) {
                if (!IntersectionTests::PointInBox(local_pos, local_space_AABB)) break;
                // The closed box's maximum faces belong to the last interior voxel.
                auto INDEX = glm::min(localToIndex(local_pos), size() - 1);
                auto mat = getVoxel(INDEX);
                if (mat > 0) {
                    return {false, INDEX, (GLubyte)mat, this, transform.apply(local_pos)};
                }
                local_pos += local_step;
            }
        }
        return {.miss = true};
    }


    void setShaderSetting(shader_setting_t setting) {
        shader_setting = setting;
    }

    void render(glm::mat4 const &parent_transform,
                glm::mat4 world_to_clip,
                glm::vec3 cam_pos,
                bool show_basis,
                float basis_length,
                float basis_width) {

        utils::opengl::debug::beginDebugGroup("VoxelVolume::render");
        // load shader program
        glUseProgram(program);

        glUniform1i(uniforms.volume, 0);
        glUniform1i(uniforms.occupancy, 1);
        upload();


        // uniforms
        auto tf = parent_transform * transform.getMatrix();
        setUniforms(tf, world_to_clip, cam_pos);

        // render
        //glEnable(GL_CULL_FACE);
        //glFrontFace(GL_CCW);
        //glCullFace(GL_FRONT);
        renderMesh(bounding_box);

        // Unbind texture and shader program
        glBindTexture(GL_TEXTURE_3D, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_3D, 0);
        glActiveTexture(GL_TEXTURE0);
        glUseProgram(0u);

        // render basis
        if (show_basis) {
            bonobo::renderBasis(basis_width, basis_length, world_to_clip, tf);
        }
        utils::opengl::debug::endDebugGroup();
    }
    
private:
    void markDirty(int x, int y, int z) {
        if (texture == 0) return;
        auto &slice = dirty_slices[z];
        if (x >= slice.min_x && x < slice.max_x &&
            y >= slice.min_y && y < slice.max_y) return;
        slice.min_x = std::min(slice.min_x, x);
        slice.min_y = std::min(slice.min_y, y);
        slice.max_x = std::max(slice.max_x, x + 1);
        slice.max_y = std::max(slice.max_y, y + 1);
        dirty = true;
    }

    std::size_t uploadOccupancy() {
        if (occupancy_texture != 0 && !coarse_dirty) return 0;
        std::size_t bytes = 0;
        constexpr GLenum settings[] = {GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH,
            GL_UNPACK_IMAGE_HEIGHT, GL_UNPACK_SKIP_PIXELS, GL_UNPACK_SKIP_ROWS, GL_UNPACK_SKIP_IMAGES};
        GLint previous[6], buffer;
        for (int i = 0; i < 6; ++i) {
            glGetIntegerv(settings[i], &previous[i]);
            glPixelStorei(settings[i], i == 0 ? 1 : 0);
        }
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &buffer);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glActiveTexture(GL_TEXTURE1);
        if (occupancy_texture == 0) {
            glGenTextures(1, &occupancy_texture);
            glBindTexture(GL_TEXTURE_3D, occupancy_texture);
            glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, coarse_size.x, coarse_size.y,
                         coarse_size.z, 0, GL_RED, GL_UNSIGNED_BYTE, occupancy.data());
            bytes = occupancy.size();
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        } else {
            glBindTexture(GL_TEXTURE_3D, occupancy_texture);
            for (int z = 0; z < coarse_size.z; ++z)
                for (int y = 0; y < coarse_size.y; ++y)
                    for (int x = 0; x < coarse_size.x;) {
                        int i = x + coarse_size.x * (y + coarse_size.y * z);
                        if (!occupancy_dirty[i]) { ++x; continue; }
                        int end = x + 1;
                        while (end < coarse_size.x && occupancy_dirty[i + end - x]) ++end;
                        glTexSubImage3D(GL_TEXTURE_3D, 0, x, y, z, end - x, 1, 1,
                                        GL_RED, GL_UNSIGNED_BYTE, occupancy.data() + i);
                        bytes += end - x;
                        x = end;
                    }
        }
        std::fill(occupancy_dirty.begin(), occupancy_dirty.end(), false);
        coarse_dirty = false;
        for (int i = 0; i < 6; ++i) glPixelStorei(settings[i], previous[i]);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer);
        return bytes;
    }

    std::size_t uploadTexture() {
        if (texture != 0 && !dirty) return 0;
        std::size_t bytes = 0;

        constexpr GLenum unpack_settings[] = {
            GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH, GL_UNPACK_IMAGE_HEIGHT,
            GL_UNPACK_SKIP_PIXELS, GL_UNPACK_SKIP_ROWS, GL_UNPACK_SKIP_IMAGES
        };
        GLint previous[6];
        for (int i = 0; i < 6; ++i) {
            glGetIntegerv(unpack_settings[i], &previous[i]);
            glPixelStorei(unpack_settings[i], i == 0 ? 1 : 0);
        }
        GLint unpack_buffer;
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack_buffer);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

        if (texture == 0) {
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_3D, texture);
            glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, W, H, D, 0, GL_RED,
                         GL_UNSIGNED_BYTE, texels);
            bytes = std::size_t(W) * H * D;
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        } else {
            // Strides address the CPU volume directly, without packing a staging copy.
            glPixelStorei(GL_UNPACK_ROW_LENGTH, W);
            glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, H);
            for (int z = 0; z < D;) {
                auto const region = dirty_slices[z];
                if (region.max_x == 0) {
                    ++z;
                    continue;
                }
                int end_z = z + 1;
                while (end_z < D && dirty_slices[end_z] == region) ++end_z;
                glTexSubImage3D(GL_TEXTURE_3D, 0, region.min_x, region.min_y, z,
                                region.max_x - region.min_x, region.max_y - region.min_y,
                                end_z - z, GL_RED, GL_UNSIGNED_BYTE,
                                texels + region.min_x + region.min_y * W + z * W * H);
                bytes += std::size_t(region.max_x - region.min_x) *
                         (region.max_y - region.min_y) * (end_z - z);
                std::fill(dirty_slices.begin() + z, dirty_slices.begin() + end_z,
                          DirtySlice{W, H, 0, 0});
                z = end_z;
            }
        }
        dirty = false;
        for (int i = 0; i < 6; ++i) glPixelStorei(unpack_settings[i], previous[i]);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack_buffer);
        return bytes;
    }

    void setUniforms(glm::mat4 const &tf,
                     glm::mat4 world_to_clip, glm::vec3 cam_pos) {
        glUniform1i(uniforms.shader, world_style ? 13 : shader_setting);
        glUniform1i(uniforms.acceleration, acceleration);
        glUniform1f(uniforms.voxel_size, voxel_size);
        glUniform1f(uniforms.lod, LOD);
        glUniform3iv(uniforms.grid_size, 1, glm::value_ptr(glm::ivec3(W, H, D)));
        glUniformMatrix4fv(uniforms.model_to_world, 1, GL_FALSE, glm::value_ptr(tf));
        auto inverse = glm::inverse(tf);
        glUniformMatrix4fv(uniforms.world_to_model, 1, GL_FALSE, glm::value_ptr(inverse));
        auto normal = glm::transpose(inverse);
        glUniformMatrix4fv(uniforms.normal_to_world, 1, GL_FALSE, glm::value_ptr(normal));
        // Default world volumes share the app's once-per-frame palette uniform.
        // An explicit per-volume palette remains an override on every draw,
        // including when other volumes sharing this program use another palette.
        if (!world_style || !colorPalette.empty()) {
            auto const &palette = colorPalette.empty() ? defaultPalette() : colorPalette;
            glUniform3fv(uniforms.palette, static_cast<GLsizei>(palette.size()),
                         glm::value_ptr(palette[0]));
        }
        glUniformMatrix4fv(uniforms.world_to_clip, 1, GL_FALSE, glm::value_ptr(world_to_clip));
        glUniform3fv(uniforms.camera, 1, glm::value_ptr(cam_pos));
        glUniform3fv(uniforms.light, 1,
                     glm::value_ptr(glm::vec3(tf * glm::vec4(-0.2f, 0.2f, 0.2f, 0))));
    }

    void renderMesh(const bonobo::mesh_data &shape) const {
        auto _vao = shape.vao;
        auto _vertices_nb = static_cast<GLsizei>(shape.vertices_nb);
        auto _indices_nb = static_cast<GLsizei>(shape.indices_nb);
        auto _drawing_mode = shape.drawing_mode;
        auto _has_indices = shape.ibo != 0u;

        if (_vao == 0u || program == 0u)
            return;

        glBindVertexArray(_vao);
        if (_has_indices)
            glDrawElements(_drawing_mode, _indices_nb, GL_UNSIGNED_INT,
                           reinterpret_cast<GLvoid const *>(0x0));
        else
            glDrawArrays(_drawing_mode, 0, _vertices_nb);
        glBindVertexArray(0u);
    }

    glm::ivec3 localToIndex(glm::vec3 local) const {
        return local * sizef();
    }
};
