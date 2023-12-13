#pragma once
#include <iostream>
#include <cmath>
#include <vector>
#include <cfloat>
#include "glm/glm.hpp"
#include "../util/noise.cpp"
#include "../util/voxel_util.cpp"
#include "core/helpers.hpp"
#include "core/opengl.hpp"
#include "../util/colorPalette.hpp"

class terrain {
private:
	int m_Width;
	int m_Depth;
	float m_Elevation;
	float m_MaxHeight = 0.0f;
	float m_MinHeight = 99999.99f;
	std::vector<float> m_TerrainTexture;
	std::vector<glm::vec3> m_ColorPalette;
	std::vector<glm::vec2> m_HeightRange = { 
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

public:
	terrain(int width, int depth, float elevation, float scale, int octave, float persistance, float lacunarity, int seed, glm::ivec2 offset = glm::vec2(0)) : m_Width(width), m_Depth(depth),
		m_Elevation(elevation) {
		//generateTerrainTexture(width/2);
		generateTerrainTextureWithWater(width/octave * scale, octave, persistance, lacunarity, seed, offset);
		// set max height
		findMaxHeight();
		findMinHeight();
		// use default colors
		generateTerrainColorPalette(colorPalette::terrainDefaultColors, m_HeightRange, glm::vec2(0, 255));
	}

	terrain(int width, int depth, float elevation, std::vector<glm::vec3>& colors, std::vector<glm::vec2>& heightRange) : m_Width(width), m_Depth(depth),
		m_Elevation(elevation), m_HeightRange(heightRange) {
		generateTerrainTexture(width, depth);
		findMaxHeight();
		findMinHeight();
		// use default colors
		generateTerrainColorPalette(colors, heightRange, glm::vec2(0, 255));
	}

	void generateTerrainTexture(float scale, int octave, float persistance = .5f, float lacunarity = 2.f, int seed = 0) {
		// allocate memory to terrain texture
		m_TerrainTexture.reserve(m_Width * m_Depth);

		float maxNoiseHeight = FLT_MIN;
		float minNoiseHeight = FLT_MAX;

		// use fbm to genereate the 2d texture
		for (int i = 0; i < m_Width; i++) {
			for (int j = 0; j < m_Depth; j++) {
				// range from -1 - 1
				//m_TerrainTexture.push_back(Noise::perlinNoise::fbm(i, j, 4, scale, persistance, lacunarity));
				// range fomr 0 - 1
				//m_TerrainTexture.push_back(Noise::perlinNoise::fbm(i, j, 4, scale, persistance, lacunarity) * .5f + .5f);
				float noiseHeight = Noise::perlinNoise::fbm(i, j, octave, scale, persistance, lacunarity, seed);
				if (noiseHeight > maxNoiseHeight) maxNoiseHeight = noiseHeight;
				else if (noiseHeight < minNoiseHeight) minNoiseHeight = noiseHeight;
				m_TerrainTexture.push_back(noiseHeight);
			}
		}

		// remap noise height from min-max to 0-1 to make more details
		for (int i = 0; i < m_Width; i++) {
			for (int j = 0; j < m_Depth; j++) {
				m_TerrainTexture[i + m_Width * j] = voxel_util::remap(m_TerrainTexture[i + j * m_Width], glm::vec2(minNoiseHeight, maxNoiseHeight), glm::vec2(0.0f, 1.0f));
			}
		}

	}

	void generateTerrainTextureWithWater(float scale, int octave, float persistance = .5f, float lacunarity = 2.f, int seed = 0, glm::ivec2 offset = glm::vec2(0)) {
		// allocate memory to terrain texture
		m_TerrainTexture.reserve(m_Width * m_Depth);

		float maxNoiseHeight = FLT_MIN;
		float minNoiseHeight = FLT_MAX;

		// use fbm to genereate the 2d texture
		for (int i = 0; i < m_Width; i++) {
			for (int j = 0; j < m_Depth; j++) {
				// range from -1 - 1
				//m_TerrainTexture.push_back(Noise::perlinNoise::fbm(i, j, 4, scale, persistance, lacunarity));
				// range fomr 0 - 1
				//m_TerrainTexture.push_back(Noise::perlinNoise::fbm(i, j, 4, scale, persistance, lacunarity) * .5f + .5f);
				float noiseHeight = Noise::perlinNoise::fbm(i, j, octave, scale, persistance, lacunarity, seed, offset);
				if (noiseHeight > maxNoiseHeight) maxNoiseHeight = noiseHeight;
				else if (noiseHeight < minNoiseHeight) minNoiseHeight = noiseHeight;
				m_TerrainTexture.push_back(noiseHeight);
			}
		}

		//std::cout << minNoiseHeight << " " << maxNoiseHeight << std::endl;

		float waterHeight = minNoiseHeight + ((maxNoiseHeight - minNoiseHeight) * std::abs(minNoiseHeight / maxNoiseHeight));
		waterHeight = voxel_util::remap(waterHeight, glm::vec2(0.0f, 1.0f), glm::vec2(0.0f, std::abs(minNoiseHeight)));

		// remap noise height from min-max to 0-1 to make more details
		for (int i = 0; i < m_Width; i++) {
			for (int j = 0; j < m_Depth; j++) {
				// remap noise height to 0 -1
				float noiseHeight = voxel_util::remap(m_TerrainTexture[i + j * m_Width], glm::vec2(minNoiseHeight, maxNoiseHeight), glm::vec2(0.0f, 1.0f));
				if (noiseHeight < waterHeight)
					m_TerrainTexture[i + m_Width * j] = waterHeight;
				else
					m_TerrainTexture[i + m_Width * j] = noiseHeight;
			}
		}
	}

	void generateTerrainColorPalette(std::vector<glm::vec3>& colors_will_copy, std::vector<glm::vec2> heightRange, glm::vec2 colorRange) {
		// remap from 0-255 to 0-1
        auto colors = std::vector<glm::vec3>(colors_will_copy.size());
        for (auto col:colors_will_copy) {
            colors.emplace_back(col);
        }
		for (int i = 0; i < colors.size(); i++) {
            if(colors[i].x > 255 || colors[i].x < 0){
                std::cout << "Color value out of range 0" << std::endl;
                //continue;
            }
            if(colors[i].y > 255 || colors[i].y < 0){
                std::cout << "Color value out of range 1" << std::endl;
                //continue;
            }
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
			std::cout << curRange << std::endl;
            if(curRange.x > 255 || curRange.x < 0){
                std::cout << "Color value out of range 2" << std::endl;
                continue;
            }
            if(curRange.y > 255 || curRange.y < 0){
                std::cout << "Color value out of range 3" << std::endl;
                continue;
            }
			// interpolate the color
			for (int j = curRange.x; j < curRange.y; j++) {
				float scale = voxel_util::remap(j, curRange, glm::vec2(0.0f, 1.0f));
				m_ColorPalette.push_back(voxel_util::lerp(colors[i + 1], colors[i + 2], scale));
			}
		}
	}

	bool isInsideTerrain(int x, float height) {
        return x < height;
		//if (x < height) return true;
		//return false;
	}

	// inside is not enough
	// must turn height into sample index
	int height2ColorIndex(int x, int y, int z, glm::vec2 colorRange) {
		float height = getHeight(x % m_Width, z % m_Depth);
		// first two range should be water
		//float colorRangeStart = m_HeightRange[2].x * 255;
        float colorRangeStart = 35;

		float colorEnd = m_HeightRange[1].x * 255;
		float waterHeight = m_MinHeight + ((m_MaxHeight - m_MinHeight) * m_MinHeight / m_MaxHeight);
		//std::cout << waterHeight << std::endl;
		if (isInsideTerrain(y, height)) {
			//wrong: glm::vec2 oldRange = glm::vec2(0, 1);
			//wrong: return std::round(voxel_util::remap(height, oldRange, colorRange));
			glm::vec2 oldRange = glm::vec2(0, m_MaxHeight);
			int index = std::round(voxel_util::remap(y, oldRange, glm::vec2(colorRangeStart, colorRange.y)));
			return index;
		}
		// for water
		else if (y < waterHeight) {
			//glm::vec2 oldRange = glm::vec2(0, waterHeight - m_MinHeight);
            // not sure what I did here but I think it is nice now.
            //int blocksAboveGround = y - height; // tried to blur edge between water bottom and sediment
            float waterColorLower = 30; //m_HeightRange[1].y * 255 * 0.8;
            float waterColorHigher = 0; //m_HeightRange[0].y * 255;
			glm::vec2 oldRange = glm::vec2(height, waterHeight);
			int index = std::round(voxel_util::remap(y, oldRange, glm::vec2(waterColorLower, waterColorHigher)));
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
		std::cout << m_MaxHeight << std::endl;
	}

	void findMinHeight() {
		for (int i = 0; i < m_Width; i++) {
			for (int j = 0; j < m_Depth; j++) {
				float height = getHeight(i, j);
				if (height < m_MinHeight)
					m_MinHeight = height;
			}
		}
		std::cout << m_MinHeight << std::endl;
	}

	std::vector<float> getTerrainTexture() {
		return m_TerrainTexture;
	}

	std::vector<glm::vec3> getTerrainColorPalette() {
		return m_ColorPalette;
	}
};