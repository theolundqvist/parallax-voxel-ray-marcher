
#pragma once
#include "../util/Direction.cpp"
#include "../util/GameObject.cpp"
#include "../util/parametric_shapes.cpp"
#include "../util/parametric_shapes.hpp"
#include "core/FPSCamera.h"
#include <cstddef>
#include <glm/gtc/type_ptr.hpp>
#include <list>

class VoxelRenderer {
public:
  VoxelRenderer(FPSCameraf *cam, ShaderProgramManager *shaderManager,
                float *elapsed_time_s) {
    camera = cam;
    this->elapsed_time_s = elapsed_time_s;

    obj = new GameObject("voxel_plane");
    obj->setMesh(parametric_shapes::createQuad(1.0f, 1.0f, 1, 1));
    obj->transform.rotateAroundX(3.14f * 0.5f);
    obj->transform.translate(glm::vec3(-1.0f, 1.0f, -0.2f));

    std::srand(std::time(nullptr));

    int cube_start = tex_size / 2 - 5;
    int cube_end = tex_size / 2 + 5;
    for (int x = 0; x < tex_size; x++) {
      for (int y = 0; y < tex_size; y++) {
        for (int z = 0; z < tex_size; z++) {
          if (x > cube_start && x < cube_end && y > cube_start &&
              y < cube_end) {
            voxel_data[x][y][z] = std::rand() % 254 + 1;
            // printf("%d\n", voxel_data[x][y][z]);
          }
          else voxel_data[x][y][z] = 0;
        }
      }
    }
    buildTexture();

    GameObject::addShaderToLibrary(
        shaderManager, "voxel", [cam, elapsed_time_s](GLuint program) {
          auto cam_pos = cam->mWorld.GetTranslation();
          glUniform1f(glGetUniformLocation(program, "elapsed_time_s"),
                      *elapsed_time_s);
          glUniform3fv(glGetUniformLocation(program, "camera_position"), 1,
                       glm::value_ptr(cam_pos));
          glUniform1f(glGetUniformLocation(program, "voxel_size"), 0.1f);
          glUniform3iv(
              glGetUniformLocation(program, "grid_size"), 1,
              glm::value_ptr(glm::ivec3(tex_size, tex_size, tex_size)));
        });
    obj->setShader("voxel");
  }

  void render(bool show_basis, float basis_length_scale,
              float basis_thickness_scale) {
    obj->setTexture("voxels", texture, GL_TEXTURE_3D);
    obj->render(camera->GetWorldToClipMatrix(), glm::mat4(1.0f), show_basis,
                basis_length_scale, basis_thickness_scale);
  }

  void buildTexture() {

    texture = 0;

    GLsizei WIDTH = tex_size;
    GLsizei HEIGHT = tex_size;
    GLsizei DEPTH = tex_size;
    GLsizei mipLevelCount = 1;

    GLubyte texels[WIDTH * HEIGHT * DEPTH];

    for (int x = 0; x < WIDTH; x++) {
      for (int y = 0; y < HEIGHT; y++) {
        for (int z = 0; z < DEPTH; z++) {
          // printf("%d, %d, %d\n",x, y, z);
          // printf("%d, %d\n", (x + WIDTH * (y + DEPTH * z)),
          // (WIDTH*HEIGHT*DEPTH));
          texels[x + WIDTH * (y + DEPTH * z)] = (GLubyte)voxel_data[x][y][z];
        }
      }
    }

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_3D, texture);
    // Allocate the storage.
    // glTexStorage3D(GL_TEXTURE_3D, mipLevelCount, GL_R8, WIDTH, HEIGHT,
    //                DEPTH);
    // glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, WIDTH,HEIGHT,DEPTH,
    // GL_UNSIGNED_BYTE )
    // Upload pixel data.
    // The first 0 refers to the mipmap level (level 0, since there's only 1)
    // The following 2 zeroes refers to the x and y offsets in case you only
    // want to specify a subrectangle. The final 0 refers to the layer index
    // offset (we start from index 0 and have 2 levels). Altogether you can
    // specify a 3D box subset of the overall texture, but only one mip level at
    // a time.
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, WIDTH, HEIGHT, DEPTH, 0, GL_RED,
                 GL_UNSIGNED_BYTE, texels);
    // glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, WIDTH, HEIGHT, DEPTH,
    // GL_RED, GL_UNSIGNED_BYTE, texels);
    // glTexImage3D(	GLenum target,
    //  	GLint level,
    //  	GLint internalformat,
    //  	GLsizei width,
    //  	GLsizei height,
    //  	GLsizei depth,
    //  	GLint border,
    //  	GLenum format,
    //  	GLenum type,
    //  	const void * data);
    // Always set reasonable texture parameters
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }

private:
  GameObject *obj;

  float *elapsed_time_s;
  float voxel_size = 0.1f;
  const static int tex_size = 30;
  GLubyte voxel_data[tex_size][tex_size][tex_size] = {0};
  GLuint texture;

  FPSCameraf *camera;
};
