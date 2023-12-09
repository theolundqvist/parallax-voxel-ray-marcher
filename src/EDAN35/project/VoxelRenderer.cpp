
#pragma once
#include "../util/Direction.cpp"
#include "../util/GameObject.cpp"
#include "../util/parametric_shapes.cpp"
#include "../util/parametric_shapes.hpp"
#include "../util/3DCA.cpp"
#include "../util/perlinNoise.cpp"
#include "core/FPSCamera.h"
#include <cstddef>
#include <glm/gtc/type_ptr.hpp>
#include <list>
#include <cmath>
#include <cstdlib>   
#include <ctime>     
#include <vector>

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

		// init texels
		// the element in texels has been init to 0
		//texels = (GLubyte*)calloc(3 * tex_size * tex_size * tex_size, sizeof(GLubyte));
		texels = (GLubyte*)calloc(tex_size * tex_size * tex_size, sizeof(GLubyte));
		// need to multiply 3 cause I want to store rgb data 
		//texels = (GLubyte*)calloc(3 * tex_size * tex_size * tex_size, sizeof(GLubyte));
		//next = (GLubyte*)calloc(3 * tex_size * tex_size * tex_size, sizeof(GLubyte));
		
		std::srand(std::time(nullptr));

		// example to use cell 
		int state = 2;
		//int state = 4;
		//int state = 5;
		//int state = 10;
		cur = CA::createCells(tex_size);
		glm::vec3 center = glm::vec3(tex_size/2);
		//CA::randomizeCells(cur, center, 30, 30, 30, 2);
		CA::randomizeCells(cur, center, tex_size, tex_size, tex_size, state);

		// setup the voxel data as first generation
		// -- if you want to draw sdf shapes, remove this for loop
		/*for (int i = 0; i < tex_size; i++) {
			for (int j = 0; j < tex_size; j++) {
				for (int k = 0; k < tex_size; k++) {
					int index = CA::convert3dIndexTo2d(i, j, k);
					voxel_data[i][j][k] = cur[index].getHp();
				}
			}
		}*/

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
				glUniform1f(glGetUniformLocation(program, "voxel_size"), 0.1f);

				// grid size
				glUniform3iv(
					glGetUniformLocation(program, "grid_size"), 1,
					glm::value_ptr(glm::ivec3(tex_size, tex_size, tex_size)));
			});
		obj->setShader("voxel");
	}

	~VoxelRenderer() { free(texels); }

	int cantor(int a, int b) { return (a + b + 1) * (a + b) / 2 + b; }

	int hash(int a, int b, int c) { return cantor(a, cantor(b, c)); }

	void render(bool show_basis, float basis_length_scale,
		float basis_thickness_scale) {
		float elapsed = *this->elapsed_time_s * 0.01f;

		// first: draw color to the cells
		//CA::drawCells(cur, CA::drawMode::pos2RGB);

		// -- if you want to draw sdf shapes, remove this for loop
		// update cur
		//int state = 2;
		//bool survival[27] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1 };
		//bool spawn[27] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0 };

		//bool survival[27] = {0,0,1,0,0,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
		//bool spawn[27] = { 0,0,0,0,1,0,1,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
		// 4-7/6-8/10
		/*int state = 10;
		bool survival[27] = { 0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
		bool spawn[27] = { 0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };*/

		// 4/4/5	
		//int state = 5;
		//bool survival[27] = { 0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
		//bool spawn[27] = { 0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

		// width, height, depth should be greater than 8
		/*int state = 3;
		bool survival[27] = { 0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
		bool spawn[27] = { 0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };*/

		// doesn't work
		//int state = 5;
		//bool survival[27] = { 0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 };
		//bool spawn[27] = { 0,0,0,0,0,1,1,1,0,0,0,0,1,1,0,1,0,0,0,0,0,0,0,0,0,0 };

		// doesn't work
		//int state = 10;
		//bool survival[27] = { 0,0,1,0,0,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
		//bool spawn[27] = { 0,0,0,0,1,0,1,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

		int state = 2;
		bool survival[27] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1 };
		bool spawn[27] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,1,0,0,0,0,0,0 };

		//int state = 2;
		//bool survival[27] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1 };
		//bool spawn[27] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0 };

		/*int state = 5;
		bool survival[27] = { 0,1,0,0,1,0,0,0,1,0,0,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1 };
		bool spawn[27] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1 };*/

		// width, height, depth should be greater than 4
		/*int state = 4;
		bool survival[27] = { 0,1,1,1,0,0,0,1,1,1,0,1,1,1,0,0,0,0,1,0,0,1,1,0,1,0,1 };
		bool spawn[27] = { 0,0,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,1,1,1,1,1,0,1 };*/

		/*int state = 10;
		bool survival[27] = { 0,0,1,0,0,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
		bool spawn[27] = { 0,0,0,0,1,0,1,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };*/

		// second: update the cell with the rules
		CA::updateCells(cur, survival, spawn, state);

		
		// update the next
		//for (int x = 0; x < tex_size; x++) {
		//	for (int y = 0; y < tex_size; y++) {
		//		for (int z = 0; z < tex_size; z++) {
		//			int index = CA::convert3dIndexTo1d(x, y, z);
		//			next[index] = cur[index].getHp();
		//		}
		//	}
		//}

		// draw function
		for (int x = 0; x < tex_size; x++) {
			for (int y = 0; y < tex_size; y++) {
				for (int z = 0; z < tex_size; z++) {
					int index = CA::convert3dIndexTo1d(x, y, z);
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
					/*float perlinValue = Noise::perlinNoise::fbm(x, z, 4, 30.0f, 0.5f, 2.0f) * 0.5f + 0.5f;
					float heightValue = tex_size * perlinValue;
					texels[index] = terrain(glm::vec3(x,y,z), heightValue);*/

					// draw the first generation
					int hp = cur[index].getHp();
					//std::cout << hp << std::endl;
					//if (hp == (state -1)) {
					if (hp != 0) {
						std::hash<std::string> hasher;
						//voxel_data[x][y][z] = (GLubyte)hasher(std::to_string(x) + std::to_string(y) +
						//	std::to_string(z)) % 255;

						texels[index] = (GLubyte)hasher(std::to_string(x) + std::to_string(y) +
							std::to_string(z)) % 255;
					}
				}
			}
		}

		buildTexture();
		obj->setTexture("voxels", texture, GL_TEXTURE_3D);
		obj->render(camera->GetWorldToClipMatrix(), glm::mat4(1.0f), show_basis,
			basis_length_scale, basis_thickness_scale);

		// 为什么直接更新cur不可以，没变化一定要用next更新voxel_data才行
		// -- if you want to draw sdf shapes, remove this for loop
		// update the voxel_data with next generation
		for (int x = 0; x < tex_size; x++) {
			for (int y = 0; y < tex_size; y++) {
				for (int z = 0; z < tex_size; z++) {
					int index = CA::convert3dIndexTo1d(x, y, z);
					//voxel_data[x][y][z] = next[x][y][z];
					//texels[index] = next[index];
					texels[index] = 0;
				}
			}
		}

		// -- if you want to draw sdf shapes, remove this for loop
		// rules 
		// update cur
		/*bool survival[27] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1 };
		bool spawn[27] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,1,0,0,0,0,0,0 };
		CA::updateCells(cur, survival, spawn, state);*/

		// update voxel_data by using cur
		/*for (int x = 0; x < tex_size; x++) {
			for (int y = 0; y < tex_size; y++) {
				for (int z = 0; z < tex_size; z++) {
					int index = CA::convert3dIndexTo2d(x, y, z);
					voxel_data[x][y][z] = cur[index].getHp();
				}
			}
		}*/
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

	//// generate a voxel terrain
	//GLubyte terrain(glm::vec3 p, std::vector<std::vector<float>> maxHeight) {
	//	if (isInsideTerrain(p, maxHeight)) {
	//		std::hash<std::string> hasher;
	//		return (GLubyte)hasher(std::to_string(p.x) + std::to_string(p.y) +
	//			std::to_string(p.z)) % 255;
	//	}
	//	else
	//		return 0;
	//}

	//bool isInsideTerrain(glm::vec3 p, std::vector<std::vector<float>> maxHeight) {
	//	//float maxHeight = height * fbm(p.x, p.z, octave);
	//	// maybe I should generate a maxHeight array
	//	float maxY = maxHeight[p.x][p.z];
	//	if (p.y > maxY)
	//		return true;
	//	else
	//		return false;
	//}

	// method 2
	GLubyte terrain(glm::vec3 p, float maxHeight) {
		if (isInsideTerrain(p, maxHeight)) {
			std::hash<std::string> hasher;
			return (GLubyte)hasher(std::to_string(p.x) + std::to_string(p.y) +
				std::to_string(p.z)) % 255;
			//return maxHeight;
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
	
	void buildTexture() {

		texture = 0;

		const GLsizei WIDTH = tex_size;
		const GLsizei HEIGHT = tex_size;
		const GLsizei DEPTH = tex_size;
		GLsizei mipLevelCount = 1;

		//GLubyte texels[WIDTH * HEIGHT * DEPTH];

		//for (int x = 0; x < WIDTH; x++) {
		//	for (int y = 0; y < HEIGHT; y++) {
		//		for (int z = 0; z < DEPTH; z++) {
		//			// printf("%d, %d, %d\n",x, y, z);
		//			// printf("%d, %d\n", (x + WIDTH * (y + DEPTH * z)),
		//			// (WIDTH*HEIGHT*DEPTH));
		//			texels[x + y * HEIGHT + z * HEIGHT * DEPTH] = voxel_data[x][y][z];
		//		}
		//	}
		//}

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
		
		// don't forget to change gl_red to gl_rgb
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
	const static int tex_size = 40;
	// new method
	GLubyte* texels;

	//GLubyte voxel_data[tex_size][tex_size][tex_size] = { 0 };
	// glubyte is unsigned char
	//GLubyte next[tex_size][tex_size][tex_size] = { 0 };
	std::vector<Cell> cur;
	GLuint texture;

	FPSCameraf* camera;
};
