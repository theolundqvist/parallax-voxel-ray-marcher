
#pragma once
#include "../util/Direction.cpp"
#include "../util/GameObject.cpp"
#include "../util/parametric_shapes.cpp"
#include "../util/parametric_shapes.hpp"
#include "core/FPSCamera.h"
#include <cstddef>
#include <glm/gtc/type_ptr.hpp>
#include <list>
#include <cmath>
#include <cstdlib>   
#include <ctime>     

class VoxelRenderer {
public:
	VoxelRenderer(FPSCameraf* cam, ShaderProgramManager* shaderManager,
		float* elapsed_time_s) {
		camera = cam;
		this->elapsed_time_s = elapsed_time_s;

		obj = new GameObject("voxel_plane");
		obj->setMesh(parametric_shapes::createQuad(1.0f, 1.0f, 1, 1));
		obj->transform.rotateAroundX(3.14f * 0.5f);
		obj->transform.translate(glm::vec3(-2.0f, 2.0f, -1.0f));
		obj->transform.setScale(glm::vec3(6.0f));

		std::srand(std::time(nullptr));

		// setup the voxel data as first generation
		for (int i = 0; i < tex_size; i++) {
			for (int j = 0; j < tex_size; j++) {
				for (int k = 0; k < tex_size; k++) {
					// sum += value; almost more than 13000 are 1

					// we need to keep first generation in a small box
					/*if (i < tex_size / 2 + 10 && i > tex_size / 2 - 10 &&
						j < tex_size / 2 + 10 && j > tex_size / 2 - 10 &&
						k < tex_size / 2 + 10 && k > tex_size / 2 - 10) {
						voxel_data[i][j][k] = std::floor(static_cast<float>(std::rand()) / RAND_MAX * 2.0f);;
					}*/

					voxel_data[i][j][k] = std::floor(static_cast<float>(std::rand()) / RAND_MAX * 2.0f);
				}
			}
		}


		GameObject::addShaderToLibrary(
			shaderManager, "voxel", [cam, elapsed_time_s](GLuint program) {
				auto cam_pos = cam->mWorld.GetTranslation();

				// elapsed time
				glUniform1f(glGetUniformLocation(program, "elapsed_time_s"),
					*elapsed_time_s);

				//cam pos
				glUniform3fv(glGetUniformLocation(program, "camera_position"), 1,
					glm::value_ptr(cam_pos));

				//voxel size
				glUniform1f(glGetUniformLocation(program, "voxel_size"), 0.5f);

				// grid size
				glUniform3iv(
					glGetUniformLocation(program, "grid_size"), 1,
					glm::value_ptr(glm::ivec3(tex_size, tex_size, tex_size)));
			});
		obj->setShader("voxel");
	}

	int cantor(int a, int b) { return (a + b + 1) * (a + b) / 2 + b; }

	int hash(int a, int b, int c) { return cantor(a, cantor(b, c)); }

	void render(bool show_basis, float basis_length_scale,
		float basis_thickness_scale) {
		float elapsed = *this->elapsed_time_s * 0.01f;
		// -- if you want to draw sdf shapes, remove this for loop
		// rules 
		for (int x = 0; x < tex_size; x++) {
			for (int y = 0; y < tex_size; y++) {
				for (int z = 0; z < tex_size; z++) {
					// generate next generation 
					// rules references: https://content.wolfram.com/uploads/sites/13/2018/02/01-3-1.pdf
					int state = voxel_data[x][y][z];
					int neighbours = countNeighbour(voxel_data, x, y, z);
					//std::cout << neighbours << std::endl;
					//if (state == 0 && ((neighbours > 12 && neighbours < 15) || (neighbours > 16 && neighbours < 20))) {
					//if (state == 0 && neighbours == 3) {
					if (state == 0 && (neighbours >13 && neighbours < 20)) {
						next[x][y][z] = 1;
					}
					else if (state == 1 && neighbours < 13) { 
						next[x][y][z] = 0;
					}
					else {
						next[x][y][z] = state;
					}
				}
			}
		}

		// draw function
		for (int x = 0; x < tex_size; x++) {
			for (int y = 0; y < tex_size; y++) {
				for (int z = 0; z < tex_size; z++) {
					// draw wave
					//voxel_data[x][y][z] = wave(elapsed, x, y, z);

					// draw cube
					//voxel_data[x][y][z] = cube(glm::vec3(x,y,z));

					// draw sphere
					//glm::vec3 center = glm::vec3(tex_size / 2 - 1);
					//voxel_data[x][y][z] = sphere(glm::vec3(x, y, z), center, 10);

					// draw octahedron
					//glm::ivec3 center = glm::ivec3(tex_size / 2 - 1);
					//voxel_data[x][y][z] = octahedron(glm::ivec3(x, y, z), center, 10);

					// draw torus
					//voxel_data[x][y][z] = torus(glm::vec3(x, y, z), glm::vec3(tex_size/2), glm::vec2(10.0, 5.0));

					// draw a line segment
					//voxel_data[x][y][z] = lineSegment(glm::vec3(tex_size / 2 - 1, tex_size/2 - 10.0f, tex_size / 2 - 1),
					//								  glm::vec3(tex_size / 2 - 1, tex_size/2 + 10.0f, tex_size / 2 - 1),
					//								  glm::vec3(x, y, z), 4.0f);

					// draw a box
					//voxel_data[x][y][z] = box(glm::vec3(x, y, z), glm::vec3(tex_size/2), glm::vec3(2.0f, 4.0f, 6.0f));

					// generate a terrain
					/*float perlinValue = fbm(x + ((float)(z + 1) / tex_size), z + ((float)(z + 1) / tex_size), 4) * 0.5f + 0.5f;
					float heightValue = 45.0f * perlinValue;
					voxel_data[x][y][z] = terrain(glm::vec3(x, y, z), heightValue);*/

					// draw the first generation
					int state = voxel_data[x][y][z];
					if (state == 1) {
						std::hash<std::string> hasher;
						voxel_data[x][y][z] = (GLubyte)hasher(std::to_string(x) + std::to_string(y) +
							std::to_string(z)) % 255;
					}
				}
			}
		}


		buildTexture();
		obj->setTexture("voxels", texture, GL_TEXTURE_3D);
		obj->render(camera->GetWorldToClipMatrix(), glm::mat4(1.0f), show_basis,
			basis_length_scale, basis_thickness_scale);

		// -- if you want to draw sdf shapes, remove this for loop
		// update the voxel_data with next generation
		for (int x = 0; x < tex_size; x++) {
			for (int y = 0; y < tex_size; y++) {
				for (int z = 0; z < tex_size; z++) {
					voxel_data[x][y][z] = next[x][y][z];
				}
			}
		}
	}

	int countNeighbour(GLubyte arr[30][30][30], int x, int y, int z) {
		int sum = 0;
		for (int i = -1; i < 2; i++) {
			for (int j = -1; j < 2; j++) {
				for (int k = -1; k < 2; k++) {
					sum += arr[(x + i + tex_size) % tex_size][(y + j + tex_size) % tex_size][(z + k + tex_size) % tex_size];
				}
			}
		}

		// remove the weight of arr[x][y][z] itself
		sum -= arr[x][y][z];
		return sum;
	}

	GLubyte wave(float elapsed, int x, int y, int z) {
		elapsed *= 0.2f;
		float maxY = (std::sin(elapsed + x * 0.3f) * 0.5f + 0.5f) * tex_size * 0.5 + tex_size / 2.0;
		//maxY += z - tex_size/2.0;
		if (y < maxY) {
			std::hash<std::string> hasher;
			return (GLubyte)hasher(std::to_string(x) + std::to_string(y) +
				std::to_string(z)) % 255;
		}
		else
			return 0;
	}

	// draw a cube
	GLubyte cube(glm::ivec3 p) {
		std::vector<size_t> boundaryPoints = { tex_size / 2 + 5, tex_size / 2 - 5 };
		// if the sampler point is inside the cube
		if (p.x < boundaryPoints[0] && p.x > boundaryPoints[1] &&
			p.y < boundaryPoints[0] && p.y > boundaryPoints[1] &&
			p.z < boundaryPoints[0] && p.z > boundaryPoints[1]) {
			std::hash<std::string> hasher;
			return (GLubyte)hasher(std::to_string(p.x) + std::to_string(p.y) +
				std::to_string(p.z)) % 255;
		}
		else
			return 0;
	}

	// draw a sphere
	GLubyte sphere(glm::vec3 p, glm::vec3 c, float r) {
		if (isInsideSphere(p, c, r)) {
			std::hash<std::string> hasher;
			return (GLubyte)hasher(std::to_string(p.x) + std::to_string(p.y) +
				std::to_string(p.z)) % 255;
		}
		else
			return 0;
	}

	bool isInsideSphere(glm::vec3 p, glm::vec3 c, float r) {
		if (glm::length(p - c) < r)
			return true;
		return false;
	}

	// using sphere to generate a cloud


	// draw a octahedron
	GLubyte octahedron(glm::ivec3 p, glm::ivec3 c, int radius) {
		if (isInsideOctahedron(p, c, radius)) {
			std::hash<std::string> hasher;
			return (GLubyte)hasher(std::to_string(p.x) + std::to_string(p.y) +
				std::to_string(p.z)) % 255;
		}
		else
			return 0;
	}

	bool isInsideOctahedron(glm::ivec3 p, glm::ivec3 c, int radius) {
		std::vector<int> length = { std::abs(p.x - c.x), std::abs(p.y - c.y), std::abs(p.z - c.z) };
		if ((length[0] + length[1] + length[2]) < radius)
			return true;
		return false;
	}

	// draw a torus
	GLubyte torus(glm::vec3 p, glm::vec3 c, glm::vec2 t) {
		if (isInsideTorus(p, c, t)) {
			std::hash<std::string> hasher;
			return (GLubyte)hasher(std::to_string(p.x) + std::to_string(p.y) +
				std::to_string(p.z)) % 255;
		}
		else
			return 0;
	}

	bool isInsideTorus(glm::vec3 p, glm::vec3 c, glm::vec2 t) {
		glm::vec3 c2p = p - c;
		// q.x will less than 0 if the point p is inside a torus
		glm::vec2 q = glm::vec2(std::abs(glm::length(glm::vec2(c2p.x, c2p.z)) - t.x), c2p.y);
		// if (glm::length(q) > t.y) ?
		if (glm::length(q) < t.y)
			return true;
		else
			return false;
	}

	// draw a line/capsule
	GLubyte lineSegment(glm::vec3 start, glm::vec3 end, glm::vec3 p, float r) {
		if (isInsideLineSegment(start, end, p, r)) {
			std::hash<std::string> hasher;
			return (GLubyte)hasher(std::to_string(p.x) + std::to_string(p.y) +
				std::to_string(p.z)) % 255;
		}
		else
			return 0;
	}

	bool isInsideLineSegment(glm::vec3 start, glm::vec3 end, glm::vec3 p, float r) {
		float h = glm::clamp(glm::dot(p - start, end - start) / glm::dot(end - start, end - start), 0.0f, 1.0f);
		float d = glm::length(p - (start - h * (start - end)));
		if (d < r)
			return true;
		else
			return false;
	}

	GLubyte box(glm::vec3 p, glm::vec3 c, glm::vec3 r) {
		if (isInsideBox(p, c, r)) {
			std::hash<std::string> hasher;
			return (GLubyte)hasher(std::to_string(p.x) + std::to_string(p.y) +
				std::to_string(p.z)) % 255;
		}
		else
			return 0;
	}

	bool isInsideBox(glm::vec3 p, glm::vec3 c, glm::vec3 r) {
		glm::vec3 c2p = p - c;
		// don't forget to add abs()
		float d = std::sqrt(std::pow(std::max(std::abs(c2p.x) - r.x, 0.0f), 2.0f) +
			std::pow(std::max(std::abs(c2p.y) - r.y, 0.0f), 2.0f) +
			std::pow(std::max(std::abs(c2p.z) - r.z, 0.0f), 2.0f));
		if (d == 0.0f)
			return true;
		else
			return false;

	}

	// draw a boxframe
	GLubyte boxFrame(glm::vec3 p, glm::vec3 c, glm::vec3 r, float e) {

	}

	// generate a voxel terrain
	GLubyte terrain(glm::vec3 p, std::vector<std::vector<float>> maxHeight) {
		if (isInsideTerrain(p, maxHeight)) {
			std::hash<std::string> hasher;
			return (GLubyte)hasher(std::to_string(p.x) + std::to_string(p.y) +
				std::to_string(p.z)) % 255;
		}
		else
			return 0;
	}

	bool isInsideTerrain(glm::vec3 p, std::vector<std::vector<float>> maxHeight) {
		//float maxHeight = height * fbm(p.x, p.z, octave);
		// maybe I should generate a maxHeight array
		float maxY = maxHeight[p.x][p.z];
		if (p.y > maxY)
			return true;
		else
			return false;
	}

	// method 2
	GLubyte terrain(glm::vec3 p, float maxHeight) {
		if (isInsideTerrain(p, maxHeight)) {
			std::hash<std::string> hasher;
			return (GLubyte)hasher(std::to_string(p.x) + std::to_string(p.y) +
				std::to_string(p.z)) % 255;
		}
		else
			return 0;
	}

	bool isInsideTerrain(glm::vec3 p, float maxHeight) {
		//float maxHeight = height * fbm(p.x, p.z, octave);
		if (p.y > maxHeight)
			return true;
		else
			return false;
	}

	//std::vector<std::vector<float>> generateMaxHeight(int width, int height, int octave, float elevation) {
	//	//std::vector<std::vector<float>> maxHeight;
	//	std::vector<std::vector<float>> maxHeight(width, std::vector<float>(height, 0.0f));
	//	for (int i = 0; i < width; i++) {
	//		for (int j = 0; j < height; j++) {
	//			// remap to 0-1
	//			float perlinValue = fbm(i + ((float)(j + 1) / tex_size), j + ((float)(j + 1) / tex_size), octave) * 0.5f + 0.5f;
	//			//std::cout << perlinValue << std::endl;
	//			float heightValue = elevation * perlinValue;
	//			//std::cout << heightValue << std::endl;
	//			maxHeight[i][j] = heightValue;
	//		}
	//	}
	//	return maxHeight;
	//}

	float opSmoothUnion(float d1, float d2, float k) {
		float h = glm::clamp(0.5f + 0.5f * (d2 - d1) / k, 0.0f, 1.0f);
		return lerp(h, d2, d1) - k * h * (1.0f - h);
	}

	// not very correct!
	float perlinNoise(float x, float y) {
		int X = (int)std::floor(x) & 0xff;
		int Y = (int)std::floor(y) & 0xff;
		// x, y始终都是0
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
		// reference中的x，y其实是大于0小于1的，我直接传进来的x，y太大了
		return perlinValue;
	}

	float perlinNoise(glm::vec2 p) {
		return perlinNoise(p.x, p.y);
	}

	float fbm(float x, float y, int octave) {
		float f = 0.0f;
		float a = 0.5f;
		for (int i = 0; i < octave; i++) {
			f += a * perlinNoise(x, y);
			x *= 2;
			y *= 2;
			a *= 0.5f;
		}
		return f;
	}

	float fbm(glm::vec2 p, int octave) {
		return fbm(p.x, p.y, octave);
	}


	float lerp(float t, float a, float b) { return (1 - t) * a + t * b; }

	// reference: https://mrl.cs.nyu.edu/~perlin/paper445.pdf
	float fade(float t) {
		return t * t * t * (t * (t * 6 - 15) + 10);
	}

	float grad(int hash, float x, float y) {
		return ((hash & 1) == 0 ? x : -x) + ((hash & 2) == 0 ? y : -y);
	}

	float grad(int hash, glm::vec2 p) {
		return grad(hash, p.x, p.y);
	}

	const std::vector<int> perm = {
	151,160,137,91,90,15,
	131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
	190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
	88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
	77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
	102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
	135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
	5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
	223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
	129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
	251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
	49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
	138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
	151
	};


	void buildTexture() {

		texture = 0;

		const GLsizei WIDTH = tex_size;
		const GLsizei HEIGHT = tex_size;
		const GLsizei DEPTH = tex_size;
		GLsizei mipLevelCount = 1;

		GLubyte texels[WIDTH * HEIGHT * DEPTH];

		for (int x = 0; x < WIDTH; x++) {
			for (int y = 0; y < HEIGHT; y++) {
				for (int z = 0; z < DEPTH; z++) {
					// printf("%d, %d, %d\n",x, y, z);
					// printf("%d, %d\n", (x + WIDTH * (y + DEPTH * z)),
					// (WIDTH*HEIGHT*DEPTH));
					texels[x + y * HEIGHT + z * HEIGHT * DEPTH] = voxel_data[x][y][z];
				}
			}
		}

		glGenTextures(1, &texture);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glBindTexture(GL_TEXTURE_3D, texture);
		// Allocate the storage.
		// glTexStorage3D(GL_TEXTURE_3D, mipLevelCount, GL_R8, WIDTH, HEIGHT,
		//                DEPTH);
		// glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, WIDTH,HEIGHT,DEPTH,
		// GL_UNSIGNED_BYTE )
		// Upload pixel data.
		// The first 0 refers to the mipmap level (level 0, since there's only 1)
		// The following 2 zeroes refers to the x and y offsets in case you only
		// want to specify a subrectangle. The final 0 refers to the layer index
		// offset (we start from index 0 and have 2 levels). Altogether you can
		// specify a 3D box subset of the overall texture, but only one mip level at
		// a time.
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, WIDTH, HEIGHT, DEPTH, 0, GL_RED,
			GL_UNSIGNED_BYTE, texels);
		// glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, WIDTH, HEIGHT, DEPTH,
		// GL_RED, GL_UNSIGNED_BYTE, texels);
		// glTexImage3D(	GLenum target,
		//  	GLint level,
		//  	GLint internalformat,
		//  	GLsizei width,
		//  	GLsizei height,
		//  	GLsizei depth,
		//  	GLint border,
		//  	GLenum format,
		//  	GLenum type,
		//  	const void * data);
		// Always set reasonable texture parameters
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	}

private:
	GameObject* obj;

	float* elapsed_time_s;
	float voxel_size = 0.1f;
	const static int tex_size = 30;
	GLubyte voxel_data[tex_size][tex_size][tex_size] = { 0 };
	GLubyte next[tex_size][tex_size][tex_size] = { 0 };
	GLuint texture;

	FPSCameraf* camera;
};
