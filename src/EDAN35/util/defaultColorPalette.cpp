#pragma
#include <iostream>
#include <vector>
#include "glm/glm.hpp"
#include "../util/voxel_util.cpp"

namespace defaultColorPalette {
	static std::vector<glm::vec3> CAColorsRed2Green = {
			glm::vec3(0.0f, 0.0f, 0.0f),
			// light red
			glm::vec3(255, 153, 153),
			// light orange
			glm::vec3(255,204,153),
			// light yellow
			glm::vec3(255,255,153),
			// green
			glm::vec3(204, 255,153),
			// light green
			glm::vec3(153, 255, 153),
	};

	static std::vector<glm::vec3> CAColorsBlue2Pink = {
		glm::vec3(153,255,255),
		// light red
		glm::vec3(153,204,255),
		// light orange
		glm::vec3(153,153,255),
		// light yellow
		glm::vec3(204,153,255),
		// light green
		glm::vec3(255,153,255),
		// light blue
		glm::vec3(255,204,229),
	};

	static std::vector<glm::vec3> terrainDefaultColors = {
		glm::vec3(0.0f, 0.0f, 0.0f),
		// water1
		glm::vec3(0.0f, 128, 255),
		// water2
		glm::vec3(51, 153, 255),
		// dirt1
		glm::vec3(240,230,140),
		// dirt2
		glm::vec3(238, 232,170),
		// grass1
		glm::vec3(0,100,0),
		// grass2
		glm::vec3(0,128,0),
		// stone1
		glm::vec3(60,40,40),
		// stone2
		glm::vec3(70,50,50),
		// snow
		glm::vec3(255,255,255),
	};

	static std::vector<glm::vec3> generateCAColorPalette(std::vector<glm::vec3>& colors, glm::vec2 colorRange) {
		// remap from 0-255 to 0-1
		for (int i = 0; i < colors.size(); i++) {
			colors[i] /= 255;
		}

		std::vector<glm::vec3> colorPalette;
		float length = (colorRange.y - colorRange.x) + 1;
		colorPalette.reserve(length);
		for (int i = 1; i < colors.size() - 1; i++) {
			glm::ivec2 curRange;
			curRange.x = (i - 1) * length / (colors.size() - 2);
			curRange.y = i * length / (colors.size() - 2);
			// interpolate the color
			for (int j = curRange.x; j < curRange.y; j++) {
				float scale = voxel_util::remap(j, curRange, glm::vec2(0.0f, 1.0f));
				colorPalette.push_back(voxel_util::lerp(colors[i], colors[i + 1], scale));
			}
		}

		/*for (int i = 0; i < colorPalette.size(); i++) {
			std::cout << colorPalette[i] << std::endl;
		}*/
		return colorPalette;
	}
}
