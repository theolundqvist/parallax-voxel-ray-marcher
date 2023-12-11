#pragma once
#include <iostream>
#include <cmath>
#include <vector>
#include <cstdlib>  
#include "glm/glm.hpp"

namespace Noise {
	static void permute(std::vector<int> perm, int n) {
		std::srand(std::time(nullptr));
		for (int i = n - 1; i > 0; i--) {
			int tempIndex = std::rand() % (i + 1);
			int temp = perm[i];
			perm[i] = perm[tempIndex];
			perm[tempIndex] = temp;
		}
	}

	static std::vector<int> gerneratePerm(int numOfPerm) {
		std::vector<int> perm;
		perm.reserve(numOfPerm);

		for (int i = 0; i < numOfPerm; i++) {
			perm.push_back(i);
		}

		permute(perm, numOfPerm);

		return perm;
	}

	static std::vector<int> perm = gerneratePerm(512);

	static class perlinNoise {
	public:
		static float perlinValue(float x, float y) {
			int X = (int)std::floor(x) & 0xff;
			int Y = (int)std::floor(y) & 0xff;
			x -= std::floor(x);
			y -= std::floor(y);
			auto u = fade(x);
			auto v = fade(y);
			auto A = (perm[X] + Y) & 0xff;
			auto B = (perm[X + 1] + Y) & 0xff;
			float a1 = grad(perm[A], x, y);
			float a2 = grad(perm[B], x - 1, y);
			float a3 = grad(perm[A + 1], x, y - 1);
			float a4 = grad(perm[B + 1], x - 1, y - 1);
			float b1 = lerp(u, a1, a2);
			float b2 = lerp(u, a3, a4);
			auto perlinValue = lerp(v, b1, b2);
			return perlinValue;
		}

		static float perlinValue(glm::vec2 p) {
			return perlinValue(p.x, p.y);
		}

		static float fbm(int x, int y, int octave,
			float scale, float persistance, float lacunarity) {
			if (scale <= 0) scale = 0.00001f;

			float accNoise = 0.0f;
			//float a = 1.0f; 
			float a = 0.5f;
			float f = 1.0f;

			for (int i = 0; i < octave; i++) {
				float sampleX = x / scale * f;
				float sampleY = y / scale * f;
				accNoise += a * perlinValue(sampleX, sampleY);

				a *= persistance;
				f *= lacunarity;
			}
			return accNoise;
		}

		static float fbm(glm::vec2 p, int octave, float scale, float persistance, float lacunarity) {
			return fbm(p.x, p.y, octave, scale, persistance, lacunarity);
		}

		static float lerp(float t, float a, float b) { return (1 - t) * a + t * b; }

		// reference: https://mrl.cs.nyu.edu/~perlin/paper445.pdf
		static float fade(float t) {
			return t * t * t * (t * (t * 6 - 15) + 10);
		}

		static float grad(int hash, float x, float y) {
			return ((hash & 1) == 0 ? x : -x) + ((hash & 2) == 0 ? y : -y);
		}

		static float grad(int hash, glm::vec2 p) {
			return grad(hash, p.x, p.y);
		}

		static glm::vec3* generateTerrainColorPalette(std::vector<glm::vec3> terrainColors,
													  std::vector<glm::vec2> heightRange) {
		}

	};
}