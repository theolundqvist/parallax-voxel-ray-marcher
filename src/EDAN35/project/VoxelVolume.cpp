#pragma once

#include "EDAN35/util/Transform.cpp"
#include "core/helpers.hpp"
#include "core/opengl.hpp"

class VoxelVolume {
private:
    GLubyte *texels;
    GLuint texture{};
    bonobo::mesh_data bounding_box;

    GLuint program{};

public:
    Transform transform;
    float voxel_size = 0.1f;
    int W;
    int H;
    int D;

    VoxelVolume(const int WIDTH, const int HEIGHT, const int DEPTH) {
        W = WIDTH;
        H = HEIGHT;
        D = DEPTH;
        texels = (GLubyte *) calloc(W * H * D, sizeof(GLubyte));
        //bounding_box = parametric_shapes::createQuad(1.0f, 1.0f, 0, 0);
        bounding_box = parametric_shapes::createCube(1.0f, 1.0f, 1.0f);

    }

    void setProgram(GLuint shaderProgram) { this->program = shaderProgram; }

    GLubyte getVoxel(int x, int y, int z) {
        return texels[x + y * W + z * W * H];
    }
    GLubyte getVoxel(glm::ivec3 index){
        return getVoxel(index.x, index.y, index.z);
    }

    void setVoxel(int x, int y, int z, GLubyte value) {
        texels[x + y * W + z * W * H] = value;
    }
    void setVoxel(glm::ivec3 index, GLubyte value) {
        setVoxel(index.x, index.y, index.z, value);
    }

    void setVolumeData(GLubyte *data) { texels = data; }

    void setVolumeData3D(GLubyte ***data) {
        texels = (GLubyte *) calloc(W * H * D, sizeof(GLubyte));
        for (int x = 0; x < W; x++) {
            for (int y = 0; y < H; y++) {
                for (int z = 0; z < D; z++) {
                    texels[x + y * W + z * W * H] = data[x][y][z];
                }
            }
        }
    }

/*
 * this is not axis aligned (aabb) since it can be rotated, and therefore does not work with most algorithms
    IntersectionTests::box_t getBoundingBox() const {
        auto min = glm::vec3(-0.5);
        // .apply just runs matrix * pos => vec3
        return {.min = transform.apply(min),
                .max = transform.apply(-min)};
    }
*/

    typedef struct voxel_hit_t {
        bool miss;
        glm::vec3 world_pos;
        glm::vec3 world_pos_far;
        glm::ivec3 index;
    } voxel_hit_t;

    voxel_hit_t raycast(glm::vec3 w_origin, glm::vec3 w_direction) {
        //check if bounding box is hit
        auto inverse = transform.get_inverse();
        auto dir = inverse.applyRotation(w_direction);
        auto hit = IntersectionTests::RayIntersectsBox(
                {.min=glm::vec3(-.5f), .max=glm::vec3(.5f)},
                {
                        inverse.apply(w_origin),
                        dir,
                        glm::vec3(1.0f)/dir
                });
        if (!hit.miss) {
            auto P = transform.apply(hit.near); //world
            auto step = w_direction * transform.getScale() * voxel_size * 0.1f;
            for (int i = 0; i < 400; ++i) {
/*
                if (glm::any(glm::lessThan(P, box.min + step)) || glm::any(glm::greaterThan(P, box.max-step))) {
                    printf("out of bounds\n");
                    break;
                }
*/
                auto INDEX = glm::ivec3((inverse.apply(P) + glm::vec3(0.5f)) * glm::vec3(W, H, D));
                if (getVoxel(INDEX) > 0) {
                    return {.miss = false, .world_pos = P, .world_pos_far = transform.apply(hit.far), .index = INDEX};
                }
                P += step;
            }
        }
        return {.miss = true};
    }


    void render(FPSCameraf *camera,
                glm::mat4 const &parent_transform, bool show_basis,
                float basis_length, float basis_width) {

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


        // uniforms
        auto tf = parent_transform * transform.getMatrix();
        setUniforms(tf, camera);

        // render
        glEnable(GL_CULL_FACE);
        glFrontFace(GL_CW);
        glCullFace(GL_BACK);
        renderMesh(bounding_box);

        // Unbind texture and shader program
        glBindTexture(GL_TEXTURE_3D, 0);
        glUseProgram(0u);

        // render basis
        if (show_basis) {
            bonobo::renderBasis(basis_width, basis_length, camera->GetWorldToClipMatrix(), tf);
        }
        utils::opengl::debug::endDebugGroup();
    }

private:
    void setUniforms(glm::mat4 const &tf,
                     FPSCameraf *camera) const {

        // voxel size
        glUniform1f(glGetUniformLocation(program, "voxel_size"), voxel_size);
        // grid size
        glUniform3iv(glGetUniformLocation(program, "grid_size"), 1,
                     glm::value_ptr(glm::ivec3(W, H, D)));

        // vertex model to world
        glUniformMatrix4fv(glGetUniformLocation(program, "model_to_world"),
                           1, GL_FALSE, glm::value_ptr(tf));

        // world to model
        glUniformMatrix4fv(
                glGetUniformLocation(program, "world_to_model"), 1, GL_FALSE,
                glm::value_ptr(glm::transpose(glm::inverse(tf))));

        // vertex to clip
        glUniformMatrix4fv(glGetUniformLocation(program, "vertex_world_to_clip"),
                           1, GL_FALSE, glm::value_ptr(camera->GetWorldToClipMatrix()));

        //cam pos
        glUniform3fv(glGetUniformLocation(program, "camera_position"), 1,
                     glm::value_ptr(camera->mWorld.GetTranslation()));

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
};
