#pragma once
#include <iostream>
#include <vector>
#include "glm/glm.hpp"
#include "../util/voxel_util.cpp"

class colorPalette {
public:
	static std::vector<glm::vec3> CAColorsRed2Green;
	static std::vector<glm::vec3> CAColorsBlue2Pink;
    static std::vector<glm::vec3> CAColorsGreen2Orange;
    static std::vector<glm::vec3> CAColorsBlue2Orange;
	static std::vector<glm::vec3> terrainDefaultColors;

	static std::vector<glm::vec3> generateCAColorPalette(std::vector<glm::vec3>& colors, glm::vec2 colorRange) {
		// remap from 0-255 to 0-1
		for (int i = 0; i < colors.size(); i++) {
            if(colors[i].x > 255 || colors[i].x < 0){
                std::cout << "Color value out of range CAColorPalette: x=" << colors[i].x << std::endl;
                //return colors;
            }
            if(colors[i].y > 255 || colors[i].y < 0){
                std::cout << "Color value out of range CAColorPalette: y=" << colors[i].y << std::endl;
                //return colors;
            }
			else colors[i] /= 255;
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
};

