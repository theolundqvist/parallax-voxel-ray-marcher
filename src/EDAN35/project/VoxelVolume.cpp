#pragma once

#include "EDAN35/util/Transform.cpp"
#include "../util/voxel_util.cpp"
#include "core/helpers.hpp"
#include "core/opengl.hpp"
#include <glm/gtx/component_wise.hpp>

class VoxelVolume {
private:
	GLubyte* texels;
	GLuint texture{};
	bonobo::mesh_data bounding_box;
	IntersectionTests::box_t local_space_AABB = { .min = glm::vec3(0.0), .max = glm::vec3(1.0) };

	GLuint program{};

	// cell color
	static std::vector<glm::vec3> colorPalette;

public:
	Transform transform;
	float voxel_size = 0.1f;
	int W;
	int H;
	int D;

	VoxelVolume(const int WIDTH, const int HEIGHT, const int DEPTH, Transform tf) {
		W = WIDTH;
		H = HEIGHT;
		D = DEPTH;
		this->transform = tf;

		voxel_size = 1.0f / (float)W;

		texels = (GLubyte*)calloc(W * H * D, sizeof(GLubyte));
		//bounding_box = parametric_shapes::createQuad(1.0f, 1.0f, 0, 0);
		bounding_box = parametric_shapes::createCube(1.0f, 1.0f, 1.0f);
	}

	~VoxelVolume() {
		free(texels);
	}

	glm::ivec3 size() const {
		return { W, H, D };
	}

	glm::vec3 sizef() const {
		return { W, H, D };
	}


	void setProgram(GLuint shaderProgram) { this->program = shaderProgram; }

	GLubyte getVoxel(int x, int y, int z) const {
		return texels[x + y * W + z * W * H];
	}

	GLubyte getVoxel(glm::ivec3 index) const {
		return getVoxel(index.x, index.y, index.z);
	}

	bool setVoxel(int x, int y, int z, GLubyte value) {
		auto i = x + y * W + z * W * H;
		if (i < 0 || i >= W * H * D) return false;
		texels[i] = value;
		return true;
	}

	bool setVoxel(int x, int y, int z, int drawMode, Cell& cell) {
		auto i = x + y * W + z * W * H;
		if (i < 0 || i >= W * H * D) return false;
		// usually from 0-255
		glm::vec2 o;
		glm::vec2 n = glm::vec2(0, 255);

		switch (drawMode)
		{
		case voxel_util::drawMode::pos2RGB:
			o = glm::vec2(0, 3 * Cell::getCellBoundary());
			//if (cell.pos2RGB(o,n) > colorPalette.size() - 1) std::cout << cell.pos2RGB(o,n) << std::endl;
			texels[i] = cell.pos2RGB(o, n);
			break;
		case voxel_util::drawMode::distance2RGB:
			o = glm::vec2(0, 1.5f * Cell::getCellBoundary());
			texels[i] = cell.distance2RGB(glm::vec3(Cell::getCellBoundary() / 2), o, n);
			break;
		case voxel_util::drawMode::hp2RGB:
			o = glm::vec2(0, Cell::getCellState() - 1);
			texels[i] = cell.hp2RGB(o, n);
			break;
		case voxel_util::drawMode::density2RGB:
			o = glm::vec2(0, 26);
			texels[i] = cell.density2RGB(o, n);
			break;
		default:
			texels[i] = cell.getHp();
			break;
		}
		return true;

	}

	bool setVoxel(glm::ivec3 index, GLubyte value) {
		return setVoxel(index.x, index.y, index.z, value);
	}

	void cleanVoxel() {
		for (int x = 0; x < W; x++) {
			for (int y = 0; y < H; y++) {
				for (int z = 0; z < D; z++) {
					setVoxel(x, y, z, 0);
				}
			}
		}
	}

	void setVolumeData(GLubyte* data) { texels = data; }

	void setVolumeData3D(GLubyte*** data) {
		texels = (GLubyte*)calloc(W * H * D, sizeof(GLubyte));
		for (int x = 0; x < W; x++) {
			for (int y = 0; y < H; y++) {
				for (int z = 0; z < D; z++) {
					texels[x + y * W + z * W * H] = data[x][y][z];
				}
			}
		}
	}

	typedef struct voxel_hit_t {
		bool miss;
		glm::ivec3 index;
		GLubyte material;
		VoxelVolume* volume;
		glm::vec3 world_pos;
	} voxel_hit_t;

	voxel_hit_t isInside(glm::vec3 world_pos) {
		auto local_pos = transform.get_inverse().apply(world_pos);
		if (!IntersectionTests::PointInBox(local_pos, local_space_AABB))
			return { .miss = true };

		auto index = localToIndex(local_pos);
		return {
				.miss = false,
				.index = index,
				.material = getVoxel(index),
				.volume = this,
				.world_pos = world_pos
		};
	}

	bool intersectsSphere(glm::vec3 center, float radius) {
		auto inverse = transform.get_inverse();
		auto local_center = inverse.apply(center);
		auto local_radius = radius / transform.getScale().x;

		return IntersectionTests::BoxIntersectsSphere(
			local_space_AABB,
			local_center,
			local_radius
		);
	}

	void setSphere(glm::vec3 center, float radius, int material) {

		auto inverse = transform.get_inverse();
		auto local_center = inverse.apply(center);
		auto local_radius = radius / transform.getScale().x;  // will have to do
		auto min = local_center - glm::vec3(local_radius);
		auto max = local_center + glm::vec3(local_radius);
		auto index_center = localToIndex(local_center);
		auto min_index = localToIndex(min);
		auto max_index = localToIndex(max);
		auto index_radius = local_radius * this->W;
		float r2 = index_radius * index_radius;
		min_index.x = std::max(min_index.x, 0);
		min_index.y = std::max(min_index.y, 0);
		min_index.z = std::max(min_index.z, 0);
		max_index.x = std::min(max_index.x, W - 1);
		max_index.y = std::min(max_index.y, H - 1);
		max_index.z = std::min(max_index.z, D - 1);
		for (int x = min_index.x; x <= max_index.x; x++) {
			for (int y = min_index.y; y <= max_index.y; y++) {
				for (int z = min_index.z; z <= max_index.z; z++) {
					auto index = glm::ivec3(x, y, z);
					if (glm::length2(glm::vec3(index - index_center)) <= r2) {
						auto mat = material == -1 ? voxel_util::hash(index) : material;
						setVoxel(index, mat);
					}
				}
			}
		}
	}


	voxel_hit_t raycast(glm::vec3 w_origin, glm::vec3 w_direction) {
		//check if bounding box is hit
		auto inverse = transform.get_inverse();
		auto dir = inverse.applyRotation(w_direction);
		auto local_origin = inverse.apply(w_origin);
		auto hit = IntersectionTests::RayIntersectsBox(
			local_space_AABB,
			{
					local_origin,
					dir,
					glm::vec3(1.0f) / dir
			});
		if (!hit.miss) {
			if (glm::length2(hit.near - hit.far) > glm::length2(local_origin - hit.far)) {
				//camera inside BB
				hit.near = local_origin;
			}
			auto P = transform.apply(hit.near); //world
			auto step = w_direction * transform.getScale() * voxel_size * 0.10f;
			int i = 0;
			for (i = 0; i < 600; ++i) {
				/*
								if (glm::any(glm::lessThan(P, box.min + step)) || glm::any(glm::greaterThan(P, box.max-step))) {
									printf("out of bounds\n");
									break;
								}
				*/
				auto INDEX = localToIndex(inverse.apply(P));
				auto mat = getVoxel(INDEX);
				if (mat > 0) {
					return { false, INDEX, mat, this, P };
				}
				P += step;
			}
		}
		return { .miss = true };
	}


	void render(glm::mat4 const& parent_transform,
		glm::mat4 world_to_clip,
		glm::vec3 cam_pos,
		bool show_basis,
		float basis_length,
		float basis_width) {

		utils::opengl::debug::beginDebugGroup("VoxelVolume::render");
		// load shader program
		glUseProgram(program);

		// generate texture object
		glGenTextures(1, &texture);

		// Set texture unit for sampler
		glUniform1i(glGetUniformLocation(program, "volume"), 0);
		// Active texture unit before use
		glActiveTexture(GL_TEXTURE0);
		// Bind 3D texture
		glBindTexture(GL_TEXTURE_3D, texture);

		// setup texture
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glBindTexture(GL_TEXTURE_3D, texture);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, W, H, D, 0, GL_RED, GL_UNSIGNED_BYTE,
			texels);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);


		// uniforms
		auto tf = parent_transform * transform.getMatrix();
		setUniforms(tf, world_to_clip, cam_pos);

		// render
		//glEnable(GL_CULL_FACE);
		//glFrontFace(GL_CW);
		//glCullFace(GL_FRONT);
		renderMesh(bounding_box);

		// Unbind texture and shader program
		glDeleteTextures(1, &texture);
		glBindTexture(GL_TEXTURE_3D, 0);
		glUseProgram(0u);

		// render basis
		if (show_basis) {
			bonobo::renderBasis(basis_width, basis_length, world_to_clip, tf);
		}
		utils::opengl::debug::endDebugGroup();
	}

	// cell color 
	// let's say max color number is 4, max range is 0-colors.size()
	static void generateColorPalette(std::vector<glm::vec3>& colors, glm::vec2 colorRange) {
		// remap from 0-255 to 0-1
		for (int i = 0; i < colors.size(); i++) {
			colors[i] /= 255;
		}

		float length = (colorRange.y - colorRange.x) + 1;
		colorPalette.reserve(length);
		for (int i = 1; i < colors.size()-1; i++) {
			glm::ivec2 curRange;
			curRange.x = (i - 1) * length / (colors.size() - 2);
			curRange.y = i * length / (colors.size() - 2);
			// print range
			std::cout << curRange << std::endl;
			// interpolate the color
			for (int j = curRange.x; j < curRange.y; j++) {
				//vector range from 1
				//colorPalette[j] = voxel_util::lerp(j, colors[i], colors[i + 1]);
				// color should range from 0 -1
				float scale = voxel_util::remap(j, curRange, glm::vec2(0.0f, 1.0f));
				colorPalette.push_back(voxel_util::lerp(scale, colors[i], colors[i+1]));
			}
		}

		/*for (int i = 0; i < colorPalette.size(); i++) {
			std::cout << colorPalette[i] << std::endl;
		}*/

		std::cout << colorPalette.size() << std::endl;
	}

private:
	void setUniforms(glm::mat4 const& tf,
		glm::mat4 world_to_clip, glm::vec3 cam_pos) const {

		// voxel size
		glUniform1f(glGetUniformLocation(program, "voxel_size"), voxel_size);
		// grid size
		glUniform3iv(glGetUniformLocation(program, "grid_size"), 1,
			glm::value_ptr(glm::ivec3(W, H, D)));

		// color paletter
		glUniform3fv(glGetUniformLocation(program, "colorPalette"), colorPalette.size(), glm::value_ptr(colorPalette[0]));

		// vertex model to world
		glUniformMatrix4fv(glGetUniformLocation(program, "model_to_world"),
			1, GL_FALSE, glm::value_ptr(tf));

		auto inverse = glm::inverse(tf);
		// world to model
		glUniformMatrix4fv(
			glGetUniformLocation(program, "world_to_model"), 1, GL_FALSE,
			glm::value_ptr(inverse));
		// normal model -> world
		// done on gpu instead, did not get it to work
/*
		glUniformMatrix3fv(glGetUniformLocation(program, "normal_model_to_world"),
						   1, GL_FALSE, glm::value_ptr(glm::transpose(glm::inverse(tf))));
*/

// vertex to clip
		glUniformMatrix4fv(glGetUniformLocation(program, "vertex_world_to_clip"),
			1, GL_FALSE, glm::value_ptr(world_to_clip));

		//cam pos
		glUniform3fv(glGetUniformLocation(program, "camera_position"), 1,
			glm::value_ptr(cam_pos));

		// light direction
		glUniform3fv(glGetUniformLocation(program, "light_direction"), 1,
			glm::value_ptr(glm::vec3(tf * glm::vec4(-0.2f, 0.2f, 0.2f, 0))));

	}

	void renderMesh(const bonobo::mesh_data& shape) const {
		auto _vao = shape.vao;
		auto _vertices_nb = static_cast<GLsizei>(shape.vertices_nb);
		auto _indices_nb = static_cast<GLsizei>(shape.indices_nb);
		auto _drawing_mode = shape.drawing_mode;
		auto _has_indices = shape.ibo != 0u;

		if (_vao == 0u || program == 0u)
			return;

		glBindVertexArray(_vao);
		if (_has_indices)
			glDrawElements(_drawing_mode, _indices_nb, GL_UNSIGNED_INT,
				reinterpret_cast<GLvoid const*>(0x0));
		else
			glDrawArrays(_drawing_mode, 0, _vertices_nb);
		glBindVertexArray(0u);
	}

	glm::ivec3 localToIndex(glm::vec3 local) const {
		return local * sizef();
	}
};

std::vector<glm::vec3> VoxelVolume::colorPalette;
