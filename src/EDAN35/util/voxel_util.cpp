#pragma once

#include <glm/glm.hpp>
#include <string>
#include <glad/glad.h>

class voxel_util {
public:
    static int hash(glm::ivec3 index) {
        std::hash<std::string> hasher;
        return (GLubyte) hasher(std::to_string(index.x) + std::to_string(index.y) +
                                std::to_string(index.z)) %
               254 + 1;
    }

    static bool wave(float offset, int x, int y, int z, int maxY = 22) {
        float surfaceY = (std::sin(offset + x * 0.3f) * 0.5f + 0.5f) * maxY * 0.5 + maxY / 2.0;
        return y < surfaceY;
    }
};
