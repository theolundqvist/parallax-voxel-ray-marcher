#pragma once
#include <iostream>
#include <cmath>
#include <vector>
#include "glm/glm.hpp"
#include "../util/perlinNoise.cpp"
#include "../util/voxel_util.cpp"
#include "core/helpers.hpp"
#include "core/opengl.hpp"

static std::vector<glm::vec3> defaultColos = {
	// dirt1
	glm::vec3(240, 230,140),
	// dirt2
	glm::vec3(238, 232,170),
	// grass1
	glm::vec3(173,255,47),
	// grass2
	glm::vec3(154,205,50),
	// stone1
	glm::vec3(60,40,40),
	// stone2
	glm::vec3(70,50,50),
	// snow
	//glm::vec3(255,255,255)
};

class terrain {
private:
	int m_Width;
	int m_Depth;
	float m_Elevation;
	std::vector<float> m_TerrainTexture;
	std::vector<glm::vec3> m_ColorPalette;

public:
	terrain(int width, int depth, float elevation) : m_Width(width), m_Depth(depth),
													 m_Elevation(elevation) {
		generateTerrainTexture(width, depth);
		// use default colors
		generateTerrainColorPalette(defaultColos, glm::vec2(0, 255));
	}

	terrain(int width, int depth, float elevation, std::vector<glm::vec3>& colors) : m_Width(width), m_Depth(depth),
																					 m_Elevation(elevation) {
		generateTerrainTexture(width, depth);
		// use default colors
		generateTerrainColorPalette(colors, glm::vec2(0, 255));
	}

	void generateTerrainTexture(int width, int depth) {
		// allocate memory to terrain texture
		m_TerrainTexture.reserve(width * depth);
		// use fbm to genereate the 2d texture
		for (int i = 0; i < width; i++) {
			for (int j = 0; j < depth; j++) {
				//m_TerrainTexture[index] = Noise::perlinNoise::fbm(i, j, 4, 5.0f, 0.5f, 2.0f);
				m_TerrainTexture.push_back(Noise::perlinNoise::fbm(i, j, 4, 5.0f, 0.5f, 2.0f)*0.5f + 0.5f);
			}
		}
	}

	void generateTerrainColorPalette(std::vector<glm::vec3>& colors, glm::vec2 colorRange) {
		// remap from 0-255 to 0-1
		for (int i = 0; i < colors.size(); i++) {
			colors[i] /= 255;
		}

		float length = (colorRange.y - colorRange.x) + 1;
		m_ColorPalette.reserve(length);
		for (int i = 0; i < colors.size()-1; i++) {
			glm::ivec2 curRange;
			curRange.x = i * length / (colors.size() - 1);
			curRange.y = (i+1) * length / (colors.size() - 1);
			std::cout << curRange << std::endl;
			for (int j = curRange.x; j < curRange.y; j++) {
				float scale = voxel_util::remap(j, curRange, glm::vec2(0.0f, 1.0f));
				m_ColorPalette.push_back(voxel_util::lerp(scale, colors[i], colors[i + 1]));
			}
		}
	}

	bool isInsideTerrain(int x, float height) {
		if (x < height) return true;
		return false;
	}

	// inside is not enough
	// must turn height into sample index
	int height2ColorIndex(int x, int y, int z, glm::vec2 colorRange) {
		float height = getHeight(x, z);
		//std::cout << height << std::endl;
		if (isInsideTerrain(y, height)) {
			glm::vec2 oldRange = glm::vec2(0, 1);
			return std::round(voxel_util::remap(height, oldRange, colorRange));
		}
		else
			// if not inside, the index is 0
			return 0;
	}

	// get the elevation of specific point
	float getHeight(float x, float y) {
		return m_TerrainTexture[x + m_Depth * y] * m_Elevation;
	}

	std::vector<float> getTerrainTexture() {
		return m_TerrainTexture;
	}

	std::vector<glm::vec3> getTerrainColorPalette() {
		return m_ColorPalette;
	}

};