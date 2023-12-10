#pragma once

#include <glm/glm.hpp>
#include <string>
#include <glad/glad.h>

class voxel_util {
public:
	static enum drawMode {
		pos2RGB = 0,
		distance2RGB = 1,
		density2RGB = 2,
		hp2RGB = 3
	};

	static int hash(glm::ivec3 index) {
		std::hash<std::string> hasher;
		return (GLubyte)hasher(std::to_string(index.x) + std::to_string(index.y) +
			std::to_string(index.z)) %
			255;
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
};
