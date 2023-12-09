
#pragma once

#include "../util/Direction.cpp"
#include "../util/GameObject.cpp"
#include "../util/voxel_util.cpp"
#include "../util/parametric_shapes.cpp"
#include "../util/parametric_shapes.hpp"
#include "../util/IntersectionTests.cpp"
#include "./VoxelVolume.cpp"
#include "core/FPSCamera.h"
#include "core/ShaderProgramManager.hpp"
#include <cstddef>
#include <cstdio>
#include <glm/gtc/type_ptr.hpp>
#include <list>

class VoxelRenderer {
public:
    VoxelRenderer(FPSCameraf *cam, ShaderProgramManager *shaderManager,
                  float *elapsed_time_s) {
        camera = cam;
        this->elapsed_time_s = elapsed_time_s;

        voxel_program = GLuint(1u);
        shaderManager->CreateAndRegisterProgram(
                "voxel",
                {{ShaderType::vertex,   "EDAN35/voxel.vert"},
                 {ShaderType::fragment, "EDAN35/voxel.frag"}},
                voxel_program);

        std::srand(std::time(nullptr));
        auto tf = Transform()
                .scale(3.0f)
                .rotateAroundX(glm::pi<float>() * 1.5f);
        //createVolume(150, tf);
        for (int x = 0; x < 3; ++x) {
            for (int z = 0; z < 3; ++z) {
                createVolume(50, tf.translated(x * 3, 0, z * 3));
            }
        }
        for (auto volume: volumes) {
            updateVolume(volume);
        }
    }

    void createVolume(int size, Transform transform = Transform()) {
        auto volume = new VoxelVolume(size, size, size);
        // does not work if not rotated like this ??, something wierd in the shader
        volume->transform = transform;
        volume->setProgram(voxel_program);
        volumes.push_back(volume);
    }

    void update(InputHandler *inputHandler, float dt) {
        // try this rotation
        //volumes[0]->transform.rotateAroundX(glm::pi<float>()*dt*0.0001f);

/*
        for (auto volume: volumes) {
            updateVolume(volume);
        }
*/

    }

    void updateVolume(VoxelVolume *volume) {
        // run whatever algorithm for updating the voxels
        auto volume_pos = volume->transform.getPos() * glm::vec3(volume->W, volume->H, volume->D);
        for (int x = 0; x < volume->W; x++) {
            for (int y = 0; y < volume->H; y++) {
                for (int z = 0; z < volume->D; z++) {
                    auto pos = glm::vec3(x, y, z) + volume_pos;
                    volume->setVoxel(x, y, z, wave(*elapsed_time_s * 0.001f, pos.x, pos.y, pos.z));
                }
            }
        }
    }



    float render(glm::mat4 world_to_clip, glm::vec3 cam_pos, bool show_basis, float basis_length, float basis_thickness) {
        // sort volumes by distance to camera
        glBeginQuery(GL_TIME_ELAPSED, elapsed_time_query);
        for (auto volume: volumes) {
            volume->render(
                    glm::mat4(1.0f),
                    world_to_clip,
                    cam_pos,
                    show_basis,
                    basis_length,
                    basis_thickness
            );
        }
        glEndQuery(GL_TIME_ELAPSED);
        glGetQueryObjectui64v(elapsed_time_query, GL_QUERY_RESULT,
                              &pass_elapsed_time);
        return (float) pass_elapsed_time / 1000000.0f;
    }

    typedef VoxelVolume::voxel_hit_t voxel_hit_t;

    voxel_hit_t findVoxel(glm::vec3 world_pos) {
        for (auto volume: volumes) {
            auto hit = volume->isInside(world_pos);
            if(!hit.miss) return hit;
        }
        return {.miss=true};
    }

    bool findAndSetVoxel(glm::vec3 world_pos, GLubyte material){
        auto hit = findVoxel(world_pos);
        if(hit.miss) return false;
        hit.volume->setVoxel(hit.index, material);
        return true;
    }

    std::list<VoxelVolume*> sphereIntersect(glm::vec3 center, float radius){
        std::list<VoxelVolume*> result;
        for (auto volume: volumes) {
            if(volume->intersectsSphere(center, radius)) result.push_back(volume);
        }
        return result;
    }


    voxel_hit_t raycast(glm::vec3 origin, glm::vec3 direction) {
        voxel_hit_t result = {true};
        auto closest_hit = glm::vec3(999999999.0f);
        for (auto volume: volumes) {
            // if volume is further away than the closest hit, skip it;
/*
            if (glm::length2(volume->transform.getPos()) >
                glm::length2(closest_hit.volume->transform.getPos()))
                continue;
*/
            auto hit = volume->raycast(origin, direction);
            if (hit.miss) continue;
            if (glm::length2(hit.world_pos - origin) < glm::length2(closest_hit - origin)) {
                result = hit;
                closest_hit = hit.world_pos;
            }
        }
        return result;
    }



    GLubyte wave(float offset, float x, float y, float z, int maxY = 22) {
        float surfaceY =
                (std::sin(offset + x * 0.3f) * 0.5f + 0.5f) * maxY * 0.5 +
                maxY / 2.0;
        if (y > surfaceY) {
            return voxel_util::hash(glm::ivec3(x, y, z));
        } else
            return 0;
    }

private:
    std::vector<VoxelVolume *> volumes;
    FPSCameraf *camera;
    GLuint voxel_program;
    float *elapsed_time_s;
    GLuint elapsed_time_query = createElapsedTimeQuery();
    GLuint64 pass_elapsed_time;

    static GLuint createElapsedTimeQuery() {
        GLuint query;
        glGenQueries(1, &query);

        if (utils::opengl::debug::isSupported()) {
            // Queries (like any other OpenGL object) need to have been used at least
            // once to ensure their resources have been allocated so we can call
            // `glObjectLabel()` on them.
            auto const register_query = [](GLuint const query) {
                glBeginQuery(GL_TIME_ELAPSED, query);
                glEndQuery(GL_TIME_ELAPSED);
            };

            register_query(query);
            utils::opengl::debug::nameObject(
                    GL_QUERY, query,
                    "GBuffer generation");
        }

        return query;
    }
};
