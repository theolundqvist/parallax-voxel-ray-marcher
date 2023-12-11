
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
#include <iostream>

class VoxelRenderer {
private:
    std::vector<VoxelVolume*> volumes;
    FPSCameraf *camera;
    GLuint voxel_program;
    float *elapsed_time_s;
    GLuint elapsed_time_query = createElapsedTimeQuery();
    GLuint64 pass_elapsed_time;
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
    }
    ~VoxelRenderer(){
        for (auto vol:volumes) {
            delete vol;
        }
        glDeleteQueries(1, &elapsed_time_query);
    }
    void add_volume(VoxelVolume *volume) {
        volumes.push_back(volume);
        volume->setProgram(voxel_program);
    }
    void remove_volumes(){
        for (auto vol:volumes) {
            delete vol;
        }
        volumes.clear();
    }
    VoxelVolume* getVolume(int index){
        return volumes[index];
    }
    void removeVolume(int index){
        auto volume = volumes[index];
        volumes.erase(volumes.begin()+index);
        delete volume;
    }
    int numberVolumes(){
        return volumes.size();
    }

    void update(InputHandler *inputHandler, float dt) {

    }

    void orbit(float dt){
        volumes[0]->transform.rotateAroundY(glm::pi<float>()*dt*0.0001f);
    }

    void setShaderSetting(shader_setting_t setting){
        for (auto volume: volumes) {
            volume->setShaderSetting(setting);
        }
    }

    float render(glm::mat4 world_to_clip, glm::vec3 cam_pos, bool show_basis, float basis_length, float basis_thickness) {
        // sort volumes by distance to camera
        glBeginQuery(GL_TIME_ELAPSED, elapsed_time_query);
        std::vector<VoxelVolume*> sorted_volumes = volumes;
        std::sort(sorted_volumes.begin(), sorted_volumes.end(), [cam_pos](VoxelVolume* a, VoxelVolume* b){
            return glm::length2(a->transform.getPos() - cam_pos) < glm::length2(b->transform.getPos() - cam_pos);
        });
        for (auto volume: sorted_volumes) {
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

private:
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
