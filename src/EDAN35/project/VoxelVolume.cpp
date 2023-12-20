#pragma once

#include "EDAN35/util/Transform.cpp"
#include "core/helpers.hpp"
#include "core/opengl.hpp"
#include <glm/gtx/component_wise.hpp>
#include "../util/colorPalette.hpp"


class VoxelVolume {
private:
    GLubyte *texels;
    GLubyte *texels_8; // still kept to update cpu mipmap to calculate texels 64, unnecessary i know
    GLubyte *texels_64;
    GLubyte *texels_512;
    GLubyte *texels_4096;
    GLubyte *texels_32k;
    GLubyte *texels_256k;
    GLubyte *sdf;
    GLuint texture{};
    //GLuint texture_8{};
    GLuint texture_64{};
    GLuint texture_512{};
    GLuint texture_4096{};
    GLuint texture_32k{};
    GLuint texture_256k{};
    GLuint sdf_texture{};
    bonobo::mesh_data bounding_box;
    IntersectionTests::box_t local_space_AABB = {.min=glm::vec3(0.0), .max=glm::vec3(1.0)};
    std::vector<glm::vec3> colorPalette = colorPalette::generateCAColorPalette(colorPalette::CAColorsBlue2Pink,
                                                                               glm::ivec2(0, 256));

    GLuint program{};
    shader_setting_t shader_setting = fixed_step_material;

public:
    Transform transform;
    float voxel_size = 0.1f;
    int LOD = 1;
    int sdf_dist = 0;
    int mipmap_levels = 2;
    int W, H, D, W_2, H_2, D_2, W_4, H_4, D_4, W_8, H_8, D_8, W_16, H_16, D_16, W_32, H_32, D_32, W_64, H_64, D_64;

    VoxelVolume(const int WIDTH, const int HEIGHT, const int DEPTH, Transform tf) {
        W = WIDTH;
        W_2 = W / 2;
        W_4 = W / 4;
        W_8 = W / 8;
        W_16 = W / 16;
        W_32 = W / 32;
        W_64 = W / 64;
        H = HEIGHT;
        H_2 = H / 2;
        H_4 = H / 4;
        H_8 = H / 8;
        H_16 = H / 16;
        H_32 = H / 32;
        H_64 = H / 64;
        D = DEPTH;
        D_2 = D / 2;
        D_4 = D / 4;
        D_8 = D / 8;
        D_16 = D / 16;
        D_32 = D / 32;
        D_64 = D / 64;
        this->transform = tf;

        voxel_size = 1.0f / (float) W;

        texels = (GLubyte *) calloc(W * H * D, sizeof(GLubyte));
        texels_8 = (GLubyte *) calloc(W_2 * H_2 * D_2, sizeof(GLubyte));
        texels_64 = (GLubyte *) calloc(W_4 * H_4 * D_4, sizeof(GLubyte));
        texels_512 = (GLubyte *) calloc(W_8 * H_8 * D_8, sizeof(GLubyte));
        texels_4096 = (GLubyte *) calloc(W_16 * H_16 * D_16, sizeof(GLubyte));
        texels_32k = (GLubyte *) calloc(W_32 * H_32 * D_32, sizeof(GLubyte));
        texels_256k = (GLubyte *) calloc(W_64 * H_64 * D_64, sizeof(GLubyte));
        //sdf = (GLubyte *) calloc(W * H * D, sizeof(GLubyte));
        //bounding_box = parametric_shapes::createQuad(1.0f, 1.0f, 0, 0);
        bounding_box = parametric_shapes::createCube(1.0f, 1.0f, 1.0f);
    }

    void setLOD(int lod) {
        LOD = lod;
    }

    ~VoxelVolume() {
        free(texels);
        free(texels_8);
        free(texels_64);
        free(texels_512);
        free(texels_4096);
        free(texels_32k);
        free(texels_256k);
    }

    glm::ivec3 size() const {
        return {W, H, D};
    }

    glm::vec3 sizef() const {
        return {W, H, D};
    }


    void setProgram(GLuint shaderProgram) { this->program = shaderProgram; }

    int getVoxel(int x, int y, int z) const {
        int index = x + y * W + z * W * H;
        if (index >= W * H * D || index < 0) return -1;
        return texels[x + y * W + z * W * H];
    }

    int getVoxel(glm::ivec3 index) const {
        return getVoxel(index.x, index.y, index.z);
    }

    bool setVoxel(int x, int y, int z, GLubyte value, bool update_sdf = true) {
        auto i = x + y * W + z * W * H;
        if (i < 0 || i >= W * H * D) return false;
        texels[i] = value;
        if (update_sdf) {
            calculateSDF(x, y, z, value == 0);
        }
        return true;
    }

    bool setVoxel(glm::ivec3 index, GLubyte value) {
        return setVoxel(index.x, index.y, index.z, value, true);
    }

    bool setSDF(int x, int y, int z, GLubyte value) {
        auto i = x + y * W + z * W * H;
        if (i < 0 || i >= W * H * D) return false;
        sdf[i] = value;
        return true;
    }

    void cleanVoxel() {
        for (int x = 0; x < W; x++) {
            for (int y = 0; y < H; y++) {
                for (int z = 0; z < D; z++) {
                    setVoxel(x, y, z, 0, false);
                    //setSDF(x, y, z, 1);
                }
            }
        }
        generateMipMaps();
    }

    void generateColorPalette(std::vector<glm::vec3> colors, glm::vec2 colorRange) {
        this->colorPalette = colorPalette::generateCAColorPalette(colors, colorRange);
    }

    void updateVoxels(std::function<GLubyte(int x, int y, int z, GLubyte previous)> const get_material) {
        for (int x = 0; x < W; x++) {
            for (int y = 0; y < H; y++) {
                for (int z = 0; z < D; z++) {
                    int prev = texels[x + y * W + z * W * H];
                    setVoxel(x, y, z, get_material(x, y, z, prev), false);
                }
            }
        }
        generateMipMaps();
        updateAllSDF(sdf_dist);
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

        auto index = localToIndex(local_pos);
        return {
                .miss=false,
                .index=index,
                .material=(GLubyte) getVoxel(index),
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
                        setVoxel(x, y, z, mat, false);
                    }
                }
            }
        }
        updateAllSDF(sdf_dist);
        generateMipMaps();
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
            auto P = transform.apply(hit.near); //world
            float step_size = 1.0 / 10;
            auto step = normalize(w_direction * transform.getScale()) * voxel_size * step_size;
            int i = 0;
            int max_step = (int) (1.0 / step_size * 1.0 / step_size * 1.0 / voxel_size) * 30;
            for (i = 0; i < max_step; ++i) {
                auto INDEX = localToIndex(inverse.apply(P));
                auto mat = getVoxel(INDEX);
                if (mat > 0) {
                    return {false, INDEX, (GLubyte) mat, this, P};
                } else if (mat == -1) {
                    return {true};
                }
                P += step;
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

        // generate texture object
        glGenTextures(1, &texture);

        // Set texture unit for sampler
        glUniform1i(glGetUniformLocation(program, "volume"), 0);
        // Active texture unit before use
        glActiveTexture(GL_TEXTURE0);
        // Bind 3D texture
        glBindTexture(GL_TEXTURE_3D, texture);

        // setup texture
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glBindTexture(GL_TEXTURE_3D, texture);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, W, H, D, 0, GL_RED, GL_UNSIGNED_BYTE,
                     texels);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

/*
        glGenTextures(1, &sdf_texture);
        glUniform1i(glGetUniformLocation(program, "volume_sdf"), 1);
        glActiveTexture(GL_TEXTURE0 + 1);
        // Bind 3D texture
        glBindTexture(GL_TEXTURE_3D, sdf_texture);

        // setup texture
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glBindTexture(GL_TEXTURE_3D, sdf_texture);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, W, H, D, 0, GL_RED, GL_UNSIGNED_BYTE,
                     sdf);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
*/
/*
        glGenTextures(1, &texture_8);
        glUniform1i(glGetUniformLocation(program, "volume_8"), 1);
        glActiveTexture(GL_TEXTURE0 + 1);
        // Bind 3D texture
        glBindTexture(GL_TEXTURE_3D, texture_8);

        // setup texture
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glBindTexture(GL_TEXTURE_3D, texture_8);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, W_2, H_2, D_2, 0, GL_RED, GL_UNSIGNED_BYTE,
                     texels_8);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
*/
        if (mipmap_levels > 0) {
            glGenTextures(1, &texture_64);
            glUniform1i(glGetUniformLocation(program, "volume_64"), 1);
            glActiveTexture(GL_TEXTURE0 + 1);
            // Bind 3D texture
            glBindTexture(GL_TEXTURE_3D, texture_64);

            // setup texture
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glBindTexture(GL_TEXTURE_3D, texture_64);
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, W_4, H_4, D_4, 0, GL_RED, GL_UNSIGNED_BYTE,
                texels_64);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        }
        if (mipmap_levels > 1) {
            glGenTextures(1, &texture_512);
            glUniform1i(glGetUniformLocation(program, "volume_512"), 2);
            glActiveTexture(GL_TEXTURE0 + 2);
            // Bind 3D texture
            glBindTexture(GL_TEXTURE_3D, texture_512);

            // setup texture
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glBindTexture(GL_TEXTURE_3D, texture_512);
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, W_8, H_8, D_8, 0, GL_RED, GL_UNSIGNED_BYTE,
                texels_512);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        }
        if (mipmap_levels > 2) {
            glGenTextures(1, &texture_4096);
            glUniform1i(glGetUniformLocation(program, "volume_4096"), 3);
            glActiveTexture(GL_TEXTURE0 + 3);
            // Bind 3D texture
            glBindTexture(GL_TEXTURE_3D, texture_4096);

            // setup texture
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glBindTexture(GL_TEXTURE_3D, texture_4096);
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, W_16, H_16, D_16, 0, GL_RED, GL_UNSIGNED_BYTE,
                texels_4096);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        }
        if (mipmap_levels > 3) {
            glGenTextures(1, &texture_32k);
            glUniform1i(glGetUniformLocation(program, "volume_32k"), 4);
            glActiveTexture(GL_TEXTURE0 + 4);
            // Bind 3D texture
            glBindTexture(GL_TEXTURE_3D, texture_32k);

            // setup texture
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glBindTexture(GL_TEXTURE_3D, texture_32k);
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, W_32, H_32, D_32, 0, GL_RED, GL_UNSIGNED_BYTE,
                texels_32k);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        }
        if (mipmap_levels > 4) {
            glGenTextures(1, &texture_256k);
            glUniform1i(glGetUniformLocation(program, "volume_256k"), 5);
            glActiveTexture(GL_TEXTURE0 + 5);
            // Bind 3D texture
            glBindTexture(GL_TEXTURE_3D, texture_256k);

            // setup texture
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glBindTexture(GL_TEXTURE_3D, texture_256k);
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, W_64, H_64, D_64, 0, GL_RED, GL_UNSIGNED_BYTE,
                texels_256k);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        }

        // uniforms
        auto tf = parent_transform * transform.getMatrix();
        setUniforms(tf, world_to_clip, cam_pos);

        // render
        //glEnable(GL_CULL_FACE);
        //glFrontFace(GL_CCW);
        //glCullFace(GL_FRONT);
        renderMesh(bounding_box);

        // Unbind texture and shader program
        glDeleteTextures(1, &texture);
        //glDeleteTextures(1, &texture_8);
        glDeleteTextures(1, &texture_64);
        glDeleteTextures(1, &texture_512);
        glDeleteTextures(1, &texture_4096);
        glDeleteTextures(1, &texture_32k);
        glDeleteTextures(1, &texture_256k);
        //glDeleteTextures(1, &sdf_texture);
        glBindTexture(GL_TEXTURE_3D, 0);
        glUseProgram(0u);

        // render basis
        if (show_basis) {
            bonobo::renderBasis(basis_width, basis_length, world_to_clip, tf);
        }
        utils::opengl::debug::endDebugGroup();
    }

private:
    void setUniforms(glm::mat4 const &tf,
                     glm::mat4 world_to_clip, glm::vec3 cam_pos) const {
        // shader manager
        glUniform1i(glGetUniformLocation(program, "Shader_manager"), shader_setting);
        // voxel size
        glUniform1f(glGetUniformLocation(program, "voxel_size"), voxel_size);
        glUniform1f(glGetUniformLocation(program, "lod"), LOD);
        glUniform1i(glGetUniformLocation(program, "use_sdf"), sdf_dist > 0 ? 1 : 0);
        glUniform1i(glGetUniformLocation(program, "mipmap_levels"), mipmap_levels);
        // grid size
        glUniform3iv(glGetUniformLocation(program, "grid_size"), 1,
                     glm::value_ptr(glm::ivec3(W, H, D)));

        // vertex model to world
        glUniformMatrix4fv(glGetUniformLocation(program, "model_to_world"),
                           1, GL_FALSE, glm::value_ptr(tf));

        auto inverse = glm::inverse(tf);
        // world to model
        glUniformMatrix4fv(
                glGetUniformLocation(program, "world_to_model"), 1, GL_FALSE,
                glm::value_ptr(inverse));
        // normal model -> world
        // done on gpu instead, did not get it to work
/*
        glUniformMatrix3fv(glGetUniformLocation(program, "normal_model_to_world"),
                           1, GL_FALSE, glm::value_ptr(glm::transpose(glm::inverse(tf))));
*/
        // color palette
        // set color palette here
        glUniform3fv(glGetUniformLocation(program, "colorPalette"), colorPalette.size(),
                     glm::value_ptr(colorPalette[0]));

        // vertex to clip
        glUniformMatrix4fv(glGetUniformLocation(program, "vertex_world_to_clip"),
                           1, GL_FALSE, glm::value_ptr(world_to_clip));

        //cam pos
        glUniform3fv(glGetUniformLocation(program, "camera_position"), 1,
                     glm::value_ptr(cam_pos));

        // light direction
        glUniform3fv(glGetUniformLocation(program, "light_direction"), 1,
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


    void generateMipMaps(){
        if (mipmap_levels < 1) return;
        for (int i = 0; i < W_2; ++i) {
            for (int j = 0; j < H_2; ++j) {
                for (int k = 0; k < D_2; ++k) {
                    int index = i + j * W_2 + k * W_2 * H_2;
                    int x = i * 2;
                    int y = j * 2;
                    int z = k * 2;
                    texels_8[index] = 0;
                    for (int l = x; l <= x+1; ++l) {
                        if(texels_8[index] == 1) break;
                        for (int m = y; m <= y+1; ++m) {
                            for (int n = z; n <= z+1; ++n) {
                                int other = l + m * W + n * W * H;
                                if(texels[other] > 0){
                                    texels_8[index] = 1;
                                    break;
                                }
                            }
                        }
                    }

                }
            }
        }
        if (mipmap_levels < 1) return;
        for (int i = 0; i < W_4; ++i) {
            for (int j = 0; j < H_4; ++j) {
                for (int k = 0; k < D_4; ++k) {
                    int index = i + j * W_4 + k * W_4 * H_4;
                    int x = i * 2;
                    int y = j * 2;
                    int z = k * 2;
                    texels_64[index] = 0;
                    for (int l = x; l <= x+1; ++l) {
                        if(texels_64[index] == 1) break;
                        for (int m = y; m <= y+1; ++m) {
                            for (int n = z; n <= z+1; ++n) {
                                int other = l + m * W_2 + n * W_2 * H_2;
                                if(texels_8[other] > 0){
                                    texels_64[index] = 1;
                                }
                            }
                        }
                    }

                }
            }
        }
        if (mipmap_levels < 2) return;
        for (int i = 0; i < W_8; ++i) {
            for (int j = 0; j < H_8; ++j) {
                for (int k = 0; k < D_8; ++k) {
                    int index = i + j * W_8 + k * W_8 * H_8;
                    int x = i * 2;
                    int y = j * 2;
                    int z = k * 2;
                    texels_512[index] = 0;
                    for (int l = x; l <= x+1; ++l) {
                        if(texels_512[index] == 1) break;
                        for (int m = y; m <= y+1; ++m) {
                            for (int n = z; n <= z+1; ++n) {
                                int other = l + m * W_4 + n * W_4 * H_4;
                                if(texels_64[other] > 0){
                                    texels_512[index] = 1;
                                }
                            }
                        }
                    }

                }
            }
        }
        if (mipmap_levels < 3) return;
        for (int i = 0; i < W_16; ++i) {
            for (int j = 0; j < H_16; ++j) {
                for (int k = 0; k < D_16; ++k) {
                    int index = i + j * W_16 + k * W_16 * H_16;
                    int x = i * 2;
                    int y = j * 2;
                    int z = k * 2;
                    texels_4096[index] = 0;
                    for (int l = x; l <= x+1; ++l) {
                        if(texels_4096[index] == 1) break;
                        for (int m = y; m <= y+1; ++m) {
                            for (int n = z; n <= z+1; ++n) {
                                int other = l + m * W_8 + n * W_8 * H_8;
                                if(texels_512[other] > 0){
                                    texels_4096[index] = 1;
                                }
                            }
                        }
                    }

                }
            }
        }
        if (mipmap_levels < 4) return;
        for (int i = 0; i < W_32; ++i) {
            for (int j = 0; j < H_32; ++j) {
                for (int k = 0; k < D_32; ++k) {
                    int index = i + j * W_32 + k * W_32 * H_32;
                    int x = i * 2;
                    int y = j * 2;
                    int z = k * 2;
                    texels_32k[index] = 0;
                    for (int l = x; l <= x+1; ++l) {
                        if(texels_32k[index] == 1) break;
                        for (int m = y; m <= y+1; ++m) {
                            for (int n = z; n <= z+1; ++n) {
                                int other = l + m * W_16 + n * W_16 * H_16;
                                if(texels_4096[other] > 0){
                                    texels_32k[index] = 1;
                                }
                            }
                        }
                    }

                }
            }
        }
        if (mipmap_levels < 5) return;
        for (int i = 0; i < W_64; ++i) {
            for (int j = 0; j < H_64; ++j) {
                for (int k = 0; k < D_64; ++k) {
                    int index = i + j * W_64 + k * W_64 * H_64;
                    int x = i * 2;
                    int y = j * 2;
                    int z = k * 2;
                    texels_256k[index] = 0;
                    for (int l = x; l <= x+1; ++l) {
                        if(texels_256k[index] == 1) break;
                        for (int m = y; m <= y+1; ++m) {
                            for (int n = z; n <= z+1; ++n) {
                                int other = l + m * W_32 + n * W_32 * H_32;
                                if(texels_32k[other] > 0){
                                    texels_256k[index] = 1;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void updateAllSDF(int dist = 1) {
        return;
        for (int x = 0; x < W; x++) {
            for (int y = 0; y < H; y++) {
                for (int z = 0; z < D; z++) {
                    updateSDF(x, y, z, dist);
                }
            }
        }
    }

    void updateSDF(int x, int y, int z, int dist = 1) {
        int index = x + y * W + z * W * H;
        sdf[index] = dist;
        if (dist == 0) return;
        for (int j = x - dist; j <= x + dist; j++) {
            for (int k = y - dist; k <= y + dist; k++) {
                for (int l = z - dist; l <= z + dist; l++) {
                    if (j < 0 || j >= W || k < 0 || k >= H || l < 0 || l >= D) continue;
                    int other = j + k * W + l * W * H;
                    if (texels[other] > 0) {
                        updateSDF(x, y, z, dist - 1);
                        return;
                    }
                }
            }
        }
    }

    void calculateSDF(int x, int y, int z, bool removed = false, int dist = 1) {
        return;
        int i = x + y * W + z * W * H;
        auto current_mat = texels[i];
        if (current_mat > 0 && removed) {
            sdf[i] = 0;
            for (int j = x - dist; j <= x + dist; ++j) {
                for (int k = y - dist; k <= y + dist; ++k) {
                    for (int l = z - dist; l <= z + dist; ++l) {
                        if (j < 0 || j >= W || k < 0 || k >= H || l < 0 || l >= D) continue;
                        int other = j + k * W + l * W * H;
                        //if(texels[other] == 0) {
                        sdf[other] = 0;
                        //   return;
                        //}
                    }
                }
            }
            return;
        } else if (current_mat == 0) {
            bool hasNeighbor = false;
            for (int j = x - dist; j <= x + dist; ++j) {
                for (int k = y - dist; k <= y + dist; ++k) {
                    for (int l = z - dist; l <= z + dist; ++l) {
                        if (j < 0 || j >= W || k < 0 || k >= H || l < 0 || l >= D) continue;
                        int other = j + k * W + l * W * H;
                        if (texels[other] > 0) {
                            hasNeighbor = true;
                            calculateSDF(j, k, l, false, dist - 1);
                        }
                    }
                }
            }
            sdf[i] = hasNeighbor ? 0 : 1;
        } else {
            sdf[i] = 0;
        }
    }
};
