#pragma once
#include <iostream>
#include <cmath>
#include <vector>
#include "glm/glm.hpp"
#include "../util/perlin.cpp"
#include "../util/voxel_util.cpp"
#include "core/helpers.hpp"
#include "core/opengl.hpp"

static std::vector<glm::vec3> defaultColors = {
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

static std::vector<glm::vec2> defaultHeightRange = {
	// water2 - water1
	glm::vec2(0.0f, 0.1f),
	// water1 - dirt1
	glm::vec2(0.1f, 0.15f),
	// dirt1 - dirt2
	glm::vec2(0.15f, 0.6f),
	// dirt2 - grass1
	glm::vec2(0.6f, 0.65f),
	// grass1 - grass2
	glm::vec2(0.65f, 0.7f),
	// grass2 - stone1
	glm::vec2(0.7f, 0.8f),
	// stone1 - stone2
	glm::vec2(0.8f, 0.85f),
	// stone2 - snow
	glm::vec2(0.85f, 1.0f),
};

static struct terrainColor
{
	std::vector<float> range;
	std::vector<glm::vec3> colors;
};

class terrain {
private:
	int m_Width;
	int m_Depth;
	float m_Elevation;
	float m_MaxHeight = 0.0f;
	float m_MinHeight = 99999.99f;
	std::vector<float> m_TerrainTexture;
	std::vector<glm::vec3> m_ColorPalette;
	std::vector<glm::vec2> m_HeightRange;

public:
	terrain(int width, int depth, float elevation) : m_Width(width), m_Depth(depth),
													 m_Elevation(elevation), m_HeightRange(defaultHeightRange) {
		generateTerrainTexture(width, depth);
		// set max height
		findMaxHeight();
		findMinHeight();
		// use default colors
		generateTerrainColorPalette(defaultColors, defaultHeightRange, glm::vec2(0, 255));
	}

	terrain(int width, int depth, float elevation, std::vector<glm::vec3>& colors, std::vector<glm::vec2>& heightRange) : m_Width(width), m_Depth(depth), 
																														  m_Elevation(elevation), m_HeightRange(heightRange) {
		generateTerrainTexture(width, depth);
		findMaxHeight();
		findMinHeight();
		// use default colors
		generateTerrainColorPalette(colors, heightRange, glm::vec2(0, 255));
	}

	void generateTerrainTexture(int width, int depth) {
		// allocate memory to terrain texture
		m_TerrainTexture.reserve(width * depth);
		// use fbm to genereate the 2d texture
		for (int i = 0; i < width; i++) {
			for (int j = 0; j < depth; j++) {
				//m_TerrainTexture[index] = Noise::perlinNoise::fbm(i, j, 4, 5.0f, 0.5f, 2.0f);
				m_TerrainTexture.push_back(Noise::perlinNoise::fbm(i, j, 4, width/2, 0.5f, 2.0f)*0.5f + 0.5f);
			}
		}
	}

	void generateTerrainColorPalette(std::vector<glm::vec3>& colors, std::vector<glm::vec2> heightRange, glm::vec2 colorRange) {
		// remap from 0-255 to 0-1
		for (int i = 0; i < colors.size(); i++) {
			colors[i] /= 255;
		}

		float length = (colorRange.y - colorRange.x) + 1;
		m_ColorPalette.reserve(length);
		for (int i = 0; i < colors.size() - 2; i++) {
			glm::ivec2 curRange;
			//curRange.x = (i - 1) * length / (colors.size() - 2);
			//curRange.y = i * length / (colors.size() - 2);
			curRange.x = heightRange[i].x * 255;
			curRange.y = heightRange[i].y * 255;

			// print range
			//std::cout << curRange << std::endl;
			// interpolate the color
			for (int j = curRange.x; j < curRange.y; j++) {
				float scale = voxel_util::remap(j, curRange, glm::vec2(0.0f, 1.0f));
				m_ColorPalette.push_back(voxel_util::lerp(scale, colors[i+1], colors[i + 2]));
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
		// first two range should be water
		float colorRangeStart = m_HeightRange[2].x * 255;
		float colorEnd = m_HeightRange[1].x * 255;
		float waterHeight = m_MinHeight + ((m_MaxHeight-m_MinHeight)* m_MinHeight / m_MaxHeight);
		//std::cout << waterHeight << std::endl;
		if (y < (waterHeight - 2.0f)) {
			return 0;
		}
		else if (isInsideTerrain(y, height)) {
			//wrong: glm::vec2 oldRange = glm::vec2(0, 1);
			//wrong: return std::round(voxel_util::remap(height, oldRange, colorRange));
			glm::vec2 oldRange = glm::vec2(0, m_MaxHeight);
			int index = std::round(voxel_util::remap(y, oldRange, glm::vec2(colorRangeStart, colorRange.y)));
			return index;
		}
		// for water
		else if (y < waterHeight && y > (waterHeight - 2.0f)) {
			//glm::vec2 oldRange = glm::vec2(0, waterHeight - m_MinHeight);
			float waterColorRangeStart = m_HeightRange[3].x * 255;
			glm::vec2 oldRange = glm::vec2(height, waterHeight);
			int index = std::round(voxel_util::remap(y, oldRange, glm::vec2(waterColorRangeStart,colorEnd)));
			return index;
		}
		else
			return 0;
	}

	// get the elevation of specific point
	float getHeight(float x, float y) {
		return m_TerrainTexture[x + m_Depth * y] * m_Elevation;
	}

	void findMaxHeight() {
		for (int i = 0; i < m_Width; i++) {
			for (int j = 0; j < m_Depth; j++) {
				float height = getHeight(i, j);
				if (height > m_MaxHeight)
					m_MaxHeight = height;
			}
		}
		//std::cout << m_MaxHeight << std::endl;
	}

	void findMinHeight() {
		for (int i = 0; i < m_Width; i++) {
			for (int j = 0; j < m_Depth; j++) {
				float height = getHeight(i, j);
				if (height < m_MinHeight)
					m_MinHeight = height;
			}
		}
		//std::cout << m_MinHeight << std::endl;
	}

	std::vector<float> getTerrainTexture() {
		return m_TerrainTexture;
	}

	std::vector<glm::vec3> getTerrainColorPalette() {
		return m_ColorPalette;
	}

};
