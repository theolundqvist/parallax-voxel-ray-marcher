
#pragma once
#include "../util/Direction.cpp"
#include "./VoxelVolume.cpp"
#include "../util/GameObject.cpp"
#include "../util/parametric_shapes.cpp"
#include "../util/parametric_shapes.hpp"
#include "core/FPSCamera.h"
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

    volume = new VoxelVolume(10,10,10);
    volume->transform.rotateAroundX(3.14f * 0.5f);
    volume->transform.translate(glm::vec3(-2.0f, 2.0f, -1.0f));
    volume->transform.setScale(glm::vec3(6.0f));

    std::srand(std::time(nullptr));

    GameObject::addShaderToLibrary(
        shaderManager, "voxel", [cam, elapsed_time_s](GLuint program) {
          auto cam_pos = cam->mWorld.GetTranslation();

          // elapsed time
          glUniform1f(glGetUniformLocation(program, "elapsed_time_s"),
                      *elapsed_time_s);

          //cam pos
          glUniform3fv(glGetUniformLocation(program, "camera_position"), 1,
                       glm::value_ptr(cam_pos));
        });

    for (int x = 0; x < volume->W; x++) {
      for (int y = 0; y < volume->H; y++) {
        for (int z = 0; z < volume->D; z++) {
          volume->setVoxel(x, y, z, wave(0.0, x, y, z));
          LogInfo("x: %d, y: %d, z: %d == %d\n", x, y, z, volume->getVoxel(x,y,z));
        }
      }
    }
    volume->setShader("voxel");
  }

  int cantor(int a, int b) { return (a + b + 1) * (a + b) / 2 + b; }

  int hash(int a, int b, int c) { return cantor(a, cantor(b, c)); }

  void render(bool show_basis, float basis_length_scale,
              float basis_thickness_scale) {
    float elapsed = *this->elapsed_time_s * 0.01f;


    volume->render(camera->GetWorldToClipMatrix(), glm::mat4(1.0f), show_basis,
                basis_length_scale, basis_thickness_scale);
  }

  GLubyte wave(float elapsed, int x, int y, int z) {
    elapsed *= 0.2f;
    float maxY = (std::sin(elapsed + x*0.3f) * 0.5f + 0.5f) * volume->H*0.5 + volume->H/2.0;
    //maxY += z - tex_size/2.0;
    if (y > maxY) {
      std::hash<std::string> hasher;
      return (GLubyte)hasher(std::to_string(x) + std::to_string(y) +
                          std::to_string(z)) %
          255;
    } else
      return 0;
  }

private:
  VoxelVolume *volume;

  float *elapsed_time_s;
  FPSCameraf *camera;
};
