#pragma once
#include "../util/GameObject.cpp"
#include "../util/parametric_shapes.hpp"
#include "core/FPSCamera.h"
#include <cstddef>
#include <glm/gtc/type_ptr.hpp>
#include <list>

class VoxelVolume : public GameObject {
private:
  GLubyte *texels;
  GLuint texture;

public:
  float voxel_size = 0.1f;
  int W;
  int H;
  int D;

  VoxelVolume(const int WIDTH, const int HEIGHT, const int DEPTH)
      : GameObject("VoxelVolume") {
    W = WIDTH;
    H = HEIGHT;
    D = DEPTH;
    texels = (GLubyte *)calloc(W * H * D, sizeof(GLubyte));
    setMesh(parametric_shapes::createQuad(1.0f, 1.0f, 1, 1));
  }


  GLubyte getVoxel(int x, int y, int z) {
    return texels[x + y * W + z * W * H];
  }

  void setVoxel(int x, int y, int z, GLubyte value) {
    texels[x + y * W + z * W * H] = value;
  }

  void setVolumeData(GLubyte *data) { texels = data; }

  void setVolumeData3D(GLubyte ***data) {
    texels = (GLubyte *)calloc(W * H * D, sizeof(GLubyte));
    for (int x = 0; x < W; x++) {
      for (int y = 0; y < H; y++) {
        for (int z = 0; z < D; z++) {
          texels[x + y * W + z * W * H] = data[x][y][z];
        }
      }
    }
  }

private:
    void _pre_render(glm::mat4 const &view_projection,
                   glm::mat4 const &parent_transform, bool show_basis,
                   float basis_length, float basis_width) override {
    texture = 0;
    glGenTextures(1, &texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_3D, texture);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, W, H, D, 0, GL_RED, GL_UNSIGNED_BYTE,
                 texels);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    // voxel size
    glUniform1f(glGetUniformLocation(*getProgram(), "voxel_size"), 0.1f);

    // grid size
    glUniform3iv(glGetUniformLocation(*getProgram(), "grid_size"), 1,
                 glm::value_ptr(glm::ivec3(W, H, D)));

    setTexture("voxels", texture, GL_TEXTURE_3D);
  }

};
