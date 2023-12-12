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
    static glm::vec3 lerp(glm::vec3 A, glm::vec3 B, float t ){
        return A*(1.f - t) + B*(t) ;
    }
    static float remap(float x, glm::vec2 oldRange, glm::vec2 newRange) {
        return (((x - oldRange.x) * (newRange.y - newRange.x)) / (oldRange.y - oldRange.x)) + newRange.x;
    }

    static glm::vec3 lerp(float scale, glm::vec3 v1, glm::vec3 v2) {
        return (scale * (v2 - v1) + v1);
    }
    static float lerp(float scale, float x, float y) {
        return (scale * (y - x) + x);
    }
private:
    static const int chunkWorldWidth = 4;
public:
    static glm::vec2 get_chunk_offset(int index){
        int width = chunkWorldWidth; // don't change, same is in terrain generator, should move to voxel_util
        return glm::vec2(index%width-width/2, index/width);
    }
};
