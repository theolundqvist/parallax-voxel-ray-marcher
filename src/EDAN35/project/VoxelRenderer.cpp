
#pragma once

#include "../util/Direction.cpp"
#include "../util/GameObject.cpp"
#include "../util/parametric_shapes.cpp"
#include "../util/parametric_shapes.hpp"
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

        volume = new VoxelVolume(30, 30, 30);
        // does not work if not rotated like this ??, something wierd in the shader
        volume->transform.rotateAroundX(glm::pi<float>()*1.5f);
        volume->transform.scale(3.0f);
        //volume->transform.translate(Direction::up * 1.0f);

        auto program = GLuint(1u);
        shaderManager->CreateAndRegisterProgram(
                "voxel",
                {{ShaderType::vertex,   "EDAN35/voxel.vert"},
                 {ShaderType::fragment, "EDAN35/voxel.frag"}},
                program);
        volume->setProgram(program);

        std::srand(std::time(nullptr));
        //updateVolume(100.0f);
    }

    void updateVolume(float elapsed) {
        for (int x = 0; x < volume->W; x++) {
            for (int y = 0; y < volume->H; y++) {
                for (int z = 0; z < volume->D; z++) {
                    volume->setVoxel(x, y, z, wave(elapsed, x, y, z));
                    // LogInfo("x: %d, y: %d, z: %d == %d\n", x, y, z,
                    // volume->getVoxel(x,y,z));
                }
            }
        }
    }


    float last_time = 0.0f;

    void render(bool show_basis, float basis_length, float basis_thickness) {
        float elapsed = *this->elapsed_time_s * 0.001f;
        float dt = elapsed - last_time;
        last_time = elapsed;

        // try this rotation
        // volume->transform.rotateAroundX(glm::pi<float>()*dt*0.01f);
        updateVolume(elapsed);
        volume->render(
                camera,
                glm::mat4(1.0f),
                show_basis,
                basis_length,
                basis_thickness
        );
    }

    int cantor(int a, int b) { return (a + b + 1) * (a + b) / 2 + b; }

    int hash(int a, int b, int c) { return cantor(a, cantor(b, c)); }

    GLubyte wave(float elapsed, int x, int y, int z) {
        elapsed *= 0.2f;
        float surfaceY =
                (std::sin(elapsed + x * 0.3f) * 0.5f + 0.5f) * volume->H * 0.5 +
                volume->H / 2.0;
        if (y > surfaceY) {
            std::hash<std::string> hasher;
            return (GLubyte) hasher(
                    std::to_string(x) +
                    std::to_string(y) +
                    std::to_string(z)
            ) % 255;
        } else
            return 0;
    }

private:
    VoxelVolume *volume;
    FPSCameraf *camera;
    float *elapsed_time_s;
};
