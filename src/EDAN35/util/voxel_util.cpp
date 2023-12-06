#pragma once

#include <glm/glm.hpp>
#include <string>
#include <glad/glad.h>

class voxel_util {
public:
  static int hash(glm::ivec3 index) {
    std::hash<std::string> hasher;
    return (GLubyte)hasher(std::to_string(index.x) + std::to_string(index.y) +
                           std::to_string(index.z)) %
           255;
  }
};
