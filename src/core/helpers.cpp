#include "config.hpp"
#include "helpers.hpp"

#include "core/Log.h"
#include "core/opengl.hpp"
#include "core/various.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <stb_image.h>

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>

namespace
{
	struct {
		GLuint shader;
		GLuint vao;
		GLuint vbo;
		GLuint ibo;
		GLsizei index_count;
		struct {
			GLuint world;
			GLuint view_proj;
			GLuint thickness_scale;
			GLuint length_scale;
		} shader_locations;
	} basis;

	GLuint debug_texture_id{ 0u };

	void setupBasisData();
	void createDebugTexture();
}

namespace local
{
	static GLuint fullscreen_shader;
	static GLuint display_vao;
	static std::array<char const*, 3> const cull_mode_labels{
		"Disabled",
		"Back faces",
		"Front faces"
	};
	static std::array<char const*, 3> const polygon_mode_labels{
		"Fill",
		"Line",
		"Point"
	};
}

void
bonobo::init()
{
	setupBasisData();
	createDebugTexture();

	glGenVertexArrays(1, &local::display_vao);
	assert(local::display_vao != 0u);
	local::fullscreen_shader = bonobo::createProgram("common/fullscreen.vert", "common/fullscreen.frag");
	if (local::fullscreen_shader == 0u)
		LogError("Failed to load \"fullscreen.vert\" and \"fullscreen.frag\"");
}

void
bonobo::deinit()
{
	glDeleteTextures(1, &debug_texture_id);
	debug_texture_id = 0u;

	glDeleteProgram(basis.shader);
	glDeleteBuffers(1, &basis.ibo);
	glDeleteBuffers(1, &basis.vbo);
	glDeleteVertexArrays(1, &basis.vao);

	glDeleteProgram(local::fullscreen_shader);
	glDeleteVertexArrays(1, &local::display_vao);
}

static std::vector<std::uint8_t>
getTextureData(std::string const& filename, std::uint32_t& width, std::uint32_t& height, bool flip)
{
	auto const channels_nb = 4u;
	stbi_set_flip_vertically_on_load_thread(flip ? 1 : 0);
	unsigned char* image_data = stbi_load(filename.c_str(), reinterpret_cast<int*>(&width), reinterpret_cast<int*>(&height), nullptr, channels_nb);
	if (image_data == nullptr) {
		LogWarning("Couldn't load or decode image file %s", filename.c_str());

		// Provide a small empty image instead in case of failure.
		width = 16;
		height = 16;
		return std::vector<unsigned char>(width * height * channels_nb);
	}

	std::vector<unsigned char> image(width * height * channels_nb);
	std::memcpy(image.data(), image_data, image.size());
	stbi_image_free(image_data);

	return image;
}

std::vector<bonobo::mesh_data>
bonobo::loadObjects(std::string const& filename)
{
	auto const scene_start_time = std::chrono::high_resolution_clock::now();

	std::vector<bonobo::mesh_data> objects;

	auto const end_of_basedir = filename.rfind("/");
	auto const parent_folder = (end_of_basedir != std::string::npos ? filename.substr(0, end_of_basedir) : ".") + "/";
	Assimp::Importer importer;
	auto const assimp_scene = importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_SortByPType | aiProcess_CalcTangentSpace);
	if (assimp_scene == nullptr || assimp_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || assimp_scene->mRootNode == nullptr) {
		LogError("Assimp failed to load \"%s\": %s", filename.c_str(), importer.GetErrorString());
		return objects;
	}

	if (assimp_scene->mNumMeshes == 0u) {
		LogError("No mesh available; loading \"%s\" must have had issues", filename.c_str());
		return objects;
	}

	LogInfo("┭ Loading \"%s\"…", filename.c_str());

	std::vector<bool> are_materials_used(assimp_scene->mNumMaterials, false);
	for (size_t j = 0; j < assimp_scene->mNumMeshes; ++j) {
		auto const assimp_object_mesh = assimp_scene->mMeshes[j];
		auto const material_id = assimp_object_mesh->mMaterialIndex;
		if (material_id >= assimp_scene->mNumMaterials)
			LogError("Mesh \"%s\" has a material index of %u, but only %u materials are present.", assimp_object_mesh->mName.C_Str(), material_id, assimp_scene->mNumMaterials);
		else
			are_materials_used[material_id] = true;
	}

	auto const materials_start_time = std::chrono::high_resolution_clock::now();
	std::vector<texture_bindings> materials_bindings(assimp_scene->mNumMaterials);
	std::vector<material_data> material_constants(assimp_scene->mNumMaterials);
	uint32_t texture_count = 0u;
	for (size_t i = 0; i < assimp_scene->mNumMaterials; ++i) {
		if (!are_materials_used[i])
			continue;

		auto const material_start_time = std::chrono::high_resolution_clock::now();
		texture_bindings& bindings = materials_bindings[i];
		material_data& constants = material_constants[i];
		auto const material = assimp_scene->mMaterials[i];

		auto const process_texture = [&bindings,&material,i,&parent_folder,&texture_count](aiTextureType type, std::string const& type_as_str, std::string const& name){
			if (material->GetTextureCount(type)) {
				auto const texture_start_time = std::chrono::high_resolution_clock::now();

				if (material->GetTextureCount(type) > 1)
					LogWarning("Material \"%s\" has more than one %s texture: discarding all but the first one.", material->GetName().C_Str(), type_as_str.c_str());
				aiString path;
				material->GetTexture(type, 0, &path);
				auto const id = bonobo::loadTexture2D(parent_folder + std::string(path.C_Str()));
				if (id == 0u) {
					LogWarning("Failed to load the %s texture for material \"%s\".", type_as_str.c_str(), material->GetName().C_Str());
					return;
				}
				bindings.emplace(name, id);
				++texture_count;

				utils::opengl::debug::nameObject(GL_TEXTURE, id, std::string(material->GetName().C_Str()) + " " + type_as_str);

				auto const texture_end_time = std::chrono::high_resolution_clock::now();
				LogTrivia("│ %s Texture \"%s\" loaded in %.3f ms",
				          bindings.size() == 1 ? "┌" : "├", path.C_Str(),
				          std::chrono::duration<float, std::milli>(texture_end_time - texture_start_time).count());
			}
		};

		aiColor3D color;

		material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
		constants.diffuse = glm::vec3(color.r, color.g, color.b);
		material->Get(AI_MATKEY_COLOR_SPECULAR, color);
		constants.specular = glm::vec3(color.r, color.g, color.b);
		material->Get(AI_MATKEY_COLOR_AMBIENT, color);
		constants.ambient = glm::vec3(color.r, color.g, color.b);
		material->Get(AI_MATKEY_COLOR_EMISSIVE, color);
		constants.emissive = glm::vec3(color.r, color.g, color.b);
		material->Get(AI_MATKEY_SHININESS, constants.shininess);
		material->Get(AI_MATKEY_REFRACTI, constants.indexOfRefraction);
		material->Get(AI_MATKEY_OPACITY, constants.opacity);

		process_texture(aiTextureType_DIFFUSE,  "diffuse",  "diffuse_texture");
		process_texture(aiTextureType_SPECULAR, "specular", "specular_texture");
		process_texture(aiTextureType_NORMALS,  "normals",  "normals_texture");
		process_texture(aiTextureType_OPACITY,  "opacity",  "opacity_texture");

		auto const material_end_time = std::chrono::high_resolution_clock::now();
		LogTrivia("│ %s Material \"%s\" loaded in %.3f ms",
		          bindings.empty() ? "╺" : "┕", material->GetName().C_Str(),
		          std::chrono::duration<float, std::milli>(material_end_time - material_start_time).count());
	}
	auto const materials_end_time = std::chrono::high_resolution_clock::now();

	auto const meshes_start_time = std::chrono::high_resolution_clock::now();
	objects.reserve(assimp_scene->mNumMeshes);
	for (size_t j = 0; j < assimp_scene->mNumMeshes; ++j) {
		auto const mesh_start_time = std::chrono::high_resolution_clock::now();

		auto const assimp_object_mesh = assimp_scene->mMeshes[j];

		if (!assimp_object_mesh->HasFaces()) {
			LogError("Unsupported mesh \"%s\": has no faces", assimp_object_mesh->mName.C_Str());
			continue;
		}
		if ((assimp_object_mesh->mPrimitiveTypes & ~static_cast<uint32_t>(aiPrimitiveType_POINT | aiPrimitiveType_NGONEncodingFlag))    != 0u
		 && (assimp_object_mesh->mPrimitiveTypes & ~static_cast<uint32_t>(aiPrimitiveType_LINE | aiPrimitiveType_NGONEncodingFlag))     != 0u
		 && (assimp_object_mesh->mPrimitiveTypes & ~static_cast<uint32_t>(aiPrimitiveType_TRIANGLE | aiPrimitiveType_NGONEncodingFlag)) != 0u) {
			LogError("Unsupported mesh \"%s\": uses multiple primitive types", assimp_object_mesh->mName.C_Str());
			continue;
		}
		if ((assimp_object_mesh->mPrimitiveTypes & static_cast<uint32_t>(aiPrimitiveType_POLYGON)) == static_cast<uint32_t>(aiPrimitiveType_POLYGON)) {
			LogError("Unsupported mesh \"%s\": uses polygons", assimp_object_mesh->mName.C_Str());
			continue;
		}
		if (!assimp_object_mesh->HasPositions()) {
			LogError("Unsupported mesh \"%s\": has no positions", assimp_object_mesh->mName.C_Str());
			continue;
		}

		bonobo::mesh_data object;
		if (assimp_object_mesh->mName.length != 0)
		{
			object.name = std::string(assimp_object_mesh->mName.C_Str());
		}

		glGenVertexArrays(1, &object.vao);
		assert(object.vao != 0u);
		glBindVertexArray(object.vao);

		auto const vertices_offset = 0u;
		auto const vertices_size = static_cast<GLsizeiptr>(assimp_object_mesh->mNumVertices * sizeof(glm::vec3));

		auto const normals_offset = vertices_size;
		auto const normals_size = assimp_object_mesh->HasNormals() ? vertices_size : 0u;

		auto const texcoords_offset = normals_offset + normals_size;
		auto const texcoords_size = assimp_object_mesh->HasTextureCoords(0u) ? vertices_size : 0u;

		auto const tangents_offset = texcoords_offset + texcoords_size;
		auto const tangents_size = assimp_object_mesh->HasTangentsAndBitangents() ? vertices_size : 0u;

		auto const binormals_offset = tangents_offset + tangents_size;
		auto const binormals_size = assimp_object_mesh->HasTangentsAndBitangents() ? vertices_size : 0u;

		auto const bo_size = static_cast<GLsizeiptr>(vertices_size
		                                            +normals_size
		                                            +texcoords_size
		                                            +tangents_size
		                                            +binormals_size
		                                            );
		glGenBuffers(1, &object.bo);
		assert(object.bo != 0u);
		glBindBuffer(GL_ARRAY_BUFFER, object.bo);
		glBufferData(GL_ARRAY_BUFFER, bo_size, nullptr, GL_STATIC_DRAW);

		glBufferSubData(GL_ARRAY_BUFFER, vertices_offset, vertices_size, static_cast<GLvoid const*>(assimp_object_mesh->mVertices));
		glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::vertices));
		glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::vertices), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(0x0));

		if (assimp_object_mesh->HasNormals()) {
			glBufferSubData(GL_ARRAY_BUFFER, normals_offset, normals_size, static_cast<GLvoid const*>(assimp_object_mesh->mNormals));
			glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::normals));
			glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::normals), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(normals_offset));
		}

		if (assimp_object_mesh->HasTextureCoords(0u)) {
			glBufferSubData(GL_ARRAY_BUFFER, texcoords_offset, texcoords_size, static_cast<GLvoid const*>(assimp_object_mesh->mTextureCoords[0u]));
			glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::texcoords));
			glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::texcoords), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(texcoords_offset));
		}

		if (assimp_object_mesh->HasTangentsAndBitangents()) {
			glBufferSubData(GL_ARRAY_BUFFER, tangents_offset, tangents_size, static_cast<GLvoid const*>(assimp_object_mesh->mTangents));
			glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::tangents));
			glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::tangents), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(tangents_offset));

			glBufferSubData(GL_ARRAY_BUFFER, binormals_offset, binormals_size, static_cast<GLvoid const*>(assimp_object_mesh->mBitangents));
			glEnableVertexAttribArray(static_cast<unsigned int>(bonobo::shader_bindings::binormals));
			glVertexAttribPointer(static_cast<unsigned int>(bonobo::shader_bindings::binormals), 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(binormals_offset));
		}

		glBindBuffer(GL_ARRAY_BUFFER, 0u);

		auto const num_vertices_per_face = assimp_object_mesh->mFaces[0u].mNumIndices;
		object.indices_nb = assimp_object_mesh->mNumFaces * num_vertices_per_face;
		auto object_indices = std::make_unique<GLuint[]>(static_cast<size_t>(object.indices_nb));
		for (size_t i = 0u; i < assimp_object_mesh->mNumFaces; ++i) {
			auto const& face = assimp_object_mesh->mFaces[i];
			assert(face.mNumIndices <= 3);
			object_indices[num_vertices_per_face * i + 0u] = face.mIndices[0u];
			if (num_vertices_per_face > 1u)
				object_indices[num_vertices_per_face * i + 1u] = face.mIndices[1u];
			if (num_vertices_per_face > 2u)
				object_indices[num_vertices_per_face * i + 2u] = face.mIndices[2u];
		}
		glGenBuffers(1, &object.ibo);
		assert(object.ibo != 0u);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, object.ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<unsigned int>(object.indices_nb) * sizeof(GL_UNSIGNED_INT), reinterpret_cast<GLvoid const*>(object_indices.get()), GL_STATIC_DRAW);
		object_indices.reset(nullptr);

		utils::opengl::debug::nameObject(GL_VERTEX_ARRAY, object.vao, object.name + " VAO");
		utils::opengl::debug::nameObject(GL_BUFFER, object.bo, object.name + " VBO");
		utils::opengl::debug::nameObject(GL_BUFFER, object.ibo, object.name + " IBO");

		glBindVertexArray(0u);
		glBindBuffer(GL_ARRAY_BUFFER, 0u);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0u);

		auto const material_id = assimp_object_mesh->mMaterialIndex;
		if (material_id < materials_bindings.size()) {
			object.bindings = materials_bindings[material_id];
			object.material = material_constants[material_id];
		}

		objects.push_back(object);

		auto const mesh_end_time = std::chrono::high_resolution_clock::now();

		std::string attributes = assimp_object_mesh->HasNormals() ? "normals" : "";
		if (!attributes.empty())
		  attributes += " | ";
		if (assimp_object_mesh->HasTangentsAndBitangents())
		  attributes += "tangents&bitangents";
		if (!attributes.empty())
		  attributes += " | ";
		if (assimp_object_mesh->HasTextureCoords(0))
		  attributes += "texture coordinates";
		LogTrivia("│ %s Mesh \"%s\" loaded with attributes [%s] in %.3f ms",
		          (assimp_scene->mNumMeshes == 1u) ? "╶" : (j == 0 ? "┌" : (j == assimp_scene->mNumMeshes - 1 ? "└" : "├")),
		          assimp_object_mesh->mName.C_Str(), attributes.c_str(),
		          std::chrono::duration<float, std::milli>(mesh_end_time - mesh_start_time).count());
	}
	auto const meshes_end_time = std::chrono::high_resolution_clock::now();

	auto const scene_end_time = std::chrono::high_resolution_clock::now();
	LogInfo("┕ Scene loaded in %.3f s: %u textures loaded in %.3f s and %zu meshes in %.3f s",
	        std::chrono::duration<float>(scene_end_time - scene_start_time).count(),
	        texture_count,
	        std::chrono::duration<float>(materials_end_time - materials_start_time).count(),
	        objects.size(),
	        std::chrono::duration<float>(meshes_end_time - meshes_start_time).count());

	return objects;
}

GLuint
bonobo::createTexture(uint32_t width, uint32_t height, GLenum target, GLint internal_format, GLenum format, GLenum type, GLvoid const* data)
{
	GLuint texture = 0u;
	glGenTextures(1, &texture);
	assert(texture != 0u);
	glBindTexture(target, texture);
	glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	switch (target) {
	case GL_TEXTURE_1D:
		glTexImage1D(target, 0, internal_format, static_cast<GLsizei>(width), 0, format, type, data);
		break;
	case GL_TEXTURE_2D:
		glTexImage2D(target, 0, internal_format, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, format, type, data);
		break;
	default:
		glDeleteTextures(1, &texture);
		LogError("Non-handled texture target: %08x.\n", target);
		return 0u;
	}
	glBindTexture(target, 0u);

	return texture;
}

GLuint
bonobo::loadTexture2D(std::string const& filename, bool generate_mipmap)
{
	std::uint32_t width, height;
	auto const data = getTextureData(filename, width, height, true);
	if (data.empty())
		return 0u;

	GLuint texture = bonobo::createTexture(width, height, GL_TEXTURE_2D, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, reinterpret_cast<GLvoid const*>(data.data()));
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, generate_mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if (generate_mipmap)
		glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0u);

	return texture;
}

GLuint
bonobo::loadTextureCubeMap(std::string const& posx, std::string const& negx,
                           std::string const& posy, std::string const& negy,
                           std::string const& posz, std::string const& negz,
                           bool generate_mipmap)
{
	GLuint texture = 0u;
	// Create an OpenGL texture object. Similarly to `glGenVertexArrays()`
	// and `glGenBuffers()` that were used in assignment 2,
	// `glGenTextures()` can create `n` texture objects at once. Here we
	// only one texture object that will contain our whole cube map.
	glGenTextures(1, /*! \todo fill me */&texture);
	assert(texture != 0u);

	// Similarly to vertex arrays and buffers, we first need to bind the
	// texture object in orther to use it. Here we will bind it to the
	// GL_TEXTURE_CUBE_MAP target to indicate we want a cube map. If you
	// look at `bonobo::loadTexture2D()` just above, you will see that
	// GL_TEXTURE_2D is used there, as we want a simple 2D-texture.
	glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

	// Set the wrapping properties of the texture; you can have a look on
	// http://docs.gl to learn more about them
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Set the minification and magnification properties of the textures;
	// you can have a look on http://docs.gl to lear more about them, or
	// attend EDAN35 in the next period ;-)
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, generate_mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	std::string dirs[6] = {posx, negx, posy, negy, posz, negz};
	int TEXTURE_CUBE_MAP[6] = {GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X, GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};
	for (int i = 0; i < 6; i++)
	{

		// We need to fill in the cube map using the images passed in as
		// argument. The function `getTextureData()` uses stb to read in the
		// image files and return a `std::vector<std::uint8_t>` containing all the
		// texels.
		std::uint32_t width, height;
		auto data = getTextureData(dirs[i], width, height, false);
		if (data.empty()) {
			glDeleteTextures(1, &texture);
			return 0u;
		}
		// With all the texels available on the CPU, we now want to push them
		// to the GPU: this is done using `glTexImage2D()` (among others). You
		// might have thought that the target used here would be the same as
		// the one passed to `glBindTexture()` or `glTexParameteri()`, similar
		// to what is done `bonobo::loadTexture2D()`. However, we want to fill
		// in a cube map, which has six different faces, so instead we specify
		// as the target the face we want to fill in. In this case, we will
		// start by filling the face sitting on the negative side of the
		// x-axis by specifying GL_TEXTURE_CUBE_MAP_NEGATIVE_X.
		glTexImage2D(TEXTURE_CUBE_MAP[i],
					/* mipmap level, you'll see that in EDAN35 */0,
					/* how are the components internally stored */GL_RGBA,
					/* the width of the cube map's face */static_cast<GLsizei>(width),
					/* the height of the cube map's face */static_cast<GLsizei>(height),
					/* must always be 0 */0,
					/* the format of the pixel data: which components are available */GL_RGBA,
					/* the type of each component */GL_UNSIGNED_BYTE,
					/* the pointer to the actual data on the CPU */reinterpret_cast<GLvoid const*>(data.data()));

		//! \todo repeat now the texture filling for the 5 remaining faces

	}
	if (generate_mipmap)
		// Generate the mipmap hierarchy; wait for EDAN35 to understand
		// what it does
		glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	glBindTexture(GL_TEXTURE_CUBE_MAP, 0u);

	return texture;
}

GLuint
bonobo::createProgram(std::string const& vert_shader_source_path, std::string const& frag_shader_source_path)
{
	auto const vertex_shader_source = utils::slurp_file(config::shaders_path(vert_shader_source_path));
	GLuint vertex_shader = utils::opengl::shader::generate_shader(GL_VERTEX_SHADER, vertex_shader_source);
	if (vertex_shader == 0u)
		return 0u;

	auto const fragment_shader_source = utils::slurp_file(config::shaders_path(frag_shader_source_path));
	GLuint fragment_shader = utils::opengl::shader::generate_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
	if (fragment_shader == 0u)
		return 0u;

	GLuint program = utils::opengl::shader::generate_program({ vertex_shader, fragment_shader });
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	return program;
}

void
bonobo::displayTexture(glm::vec2 const& lower_left, glm::vec2 const& upper_right, GLuint texture, GLuint sampler, glm::ivec4 const& swizzle, glm::ivec2 const& window_size, bool linearise, float nearPlane, float farPlane)
{
	auto const relative_to_absolute = [](float coord, int size) {
		return static_cast<GLint>((coord + 1.0f) / 2.0f * size);
	};
	auto const viewport_origin = glm::ivec2(relative_to_absolute(lower_left.x, window_size.x),
	                                        relative_to_absolute(lower_left.y, window_size.y));
	auto const viewport_size = glm::ivec2(relative_to_absolute(upper_right.x, window_size.x),
	                                      relative_to_absolute(upper_right.y, window_size.y))
	                         - viewport_origin;

	glViewport(viewport_origin.x, viewport_origin.y, viewport_size.x, viewport_size.y);
	glUseProgram(local::fullscreen_shader);
	glBindVertexArray(local::display_vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glBindSampler(0, sampler);
	glUniform1i(glGetUniformLocation(local::fullscreen_shader, "tex"), 0);
	glUniform4iv(glGetUniformLocation(local::fullscreen_shader, "swizzle"), 1, glm::value_ptr(swizzle));
	glUniform1i(glGetUniformLocation(local::fullscreen_shader, "linearise"), linearise);
	glUniform1f(glGetUniformLocation(local::fullscreen_shader, "near"), nearPlane);
	glUniform1f(glGetUniformLocation(local::fullscreen_shader, "far"), farPlane);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindSampler(0, 0u);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
}

GLuint
bonobo::createFBO(std::vector<GLuint> const& color_attachments, GLuint depth_attachment)
{
	auto const attach = [](GLenum attach_point, GLuint attachment){
		glFramebufferTexture2D(GL_FRAMEBUFFER, attach_point, GL_TEXTURE_2D, attachment, 0);
		auto const status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
			LogError("Failed to attach %u at %u", attachment, attach_point);
	};

	GLuint fbo = 0u;
	glGenFramebuffers(1, &fbo);
	assert(fbo != 0u);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	for (size_t i = 0; i < color_attachments.size(); ++i)
		attach(static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i), color_attachments[i]);
	if (depth_attachment != 0u)
		attach(GL_DEPTH_ATTACHMENT, depth_attachment);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return fbo;
}

GLuint
bonobo::createSampler(std::function<void (GLuint)> const& setup)
{
	GLuint sampler = 0u;
	glGenSamplers(1, &sampler);
	assert(sampler != 0u);
	setup(sampler);
	return sampler;
}

void
bonobo::drawFullscreen()
{
	glBindVertexArray(local::display_vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0u);
}

GLuint
bonobo::getDebugTextureID()
{
	return debug_texture_id;
}

void
bonobo::renderBasis(float thickness_scale, float length_scale, glm::mat4 const& view_projection, glm::mat4 const& world)
{
	if (basis.shader == 0u)
		return;

	glUseProgram(basis.shader);
	glBindVertexArray(basis.vao);
	glUniformMatrix4fv(basis.shader_locations.world, 1, GL_FALSE, glm::value_ptr(world));
	glUniformMatrix4fv(basis.shader_locations.view_proj, 1, GL_FALSE, glm::value_ptr(view_projection));
	glUniform1f(basis.shader_locations.thickness_scale, thickness_scale);
	glUniform1f(basis.shader_locations.length_scale, length_scale);
	glDrawElementsInstanced(GL_TRIANGLES, basis.index_count, GL_UNSIGNED_INT, nullptr, 3);
	glBindVertexArray(0u);
	glUseProgram(0u);
}

bool
bonobo::uiSelectCullMode(std::string const& label, enum cull_mode_t& cull_mode) noexcept
{
	auto cull_mode_index = static_cast<int>(cull_mode);
	bool was_modified = ImGui::Combo(label.c_str(), &cull_mode_index,
	                                 local::cull_mode_labels.data(),
	                                 static_cast<int>(local::cull_mode_labels.size()));
	cull_mode = static_cast<cull_mode_t>(cull_mode_index);
	return was_modified;
}

void
bonobo::changeCullMode(enum cull_mode_t const cull_mode) noexcept
{
	switch (cull_mode) {
		case bonobo::cull_mode_t::disabled:
			glDisable(GL_CULL_FACE);
			break;
		case bonobo::cull_mode_t::back_faces:
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
			break;
		case bonobo::cull_mode_t::front_faces:
			glEnable(GL_CULL_FACE);
			glCullFace(GL_FRONT);
			break;
	}
}

bool
bonobo::uiSelectPolygonMode(std::string const& label, enum polygon_mode_t& polygon_mode) noexcept
{
	auto polygon_mode_index = static_cast<int>(polygon_mode);
	bool was_modified = ImGui::Combo(label.c_str(), &polygon_mode_index,
	                                 local::polygon_mode_labels.data(),
	                                 static_cast<int>(local::polygon_mode_labels.size()));
	polygon_mode = static_cast<polygon_mode_t>(polygon_mode_index);
	return was_modified;
}

void
bonobo::changePolygonMode(enum polygon_mode_t const polygon_mode) noexcept
{
	switch (polygon_mode) {
		case bonobo::polygon_mode_t::fill:
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			break;
		case bonobo::polygon_mode_t::line:
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			break;
		case bonobo::polygon_mode_t::point:
			glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
			break;
	}
}

namespace
{
	void setupBasisData()
	{
		glGenVertexArrays(1, &basis.vao);
		assert(basis.vao != 0);
		glBindVertexArray(basis.vao);

		glGenBuffers(1, &basis.vbo);
		assert(basis.vbo != 0);
		auto const halfThickness = 0.1f;
		std::array<glm::vec3, 13> const vertices = {
			// Body of the arrow
			glm::vec3(0.0f, -halfThickness, -halfThickness),
			glm::vec3(0.0f, -halfThickness,  halfThickness),
			glm::vec3(0.0f,  halfThickness,  halfThickness),
			glm::vec3(0.0f,  halfThickness, -halfThickness),
			glm::vec3(1.0f, -halfThickness, -halfThickness),
			glm::vec3(1.0f, -halfThickness,  halfThickness),
			glm::vec3(1.0f,  halfThickness,  halfThickness),
			glm::vec3(1.0f,  halfThickness, -halfThickness),
			// Tip of the arrow
			glm::vec3(1.0f, -2.0f*halfThickness, -2.0f*halfThickness),
			glm::vec3(1.0f, -2.0f*halfThickness,  2.0f*halfThickness),
			glm::vec3(1.0f,  2.0f*halfThickness,  2.0f*halfThickness),
			glm::vec3(1.0f,  2.0f*halfThickness, -2.0f*halfThickness),
			glm::vec3(1.0f+4.0f*halfThickness,  0.0f, 0.0f),
		};
		glBindBuffer(GL_ARRAY_BUFFER, basis.vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(0u);
		glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(0x0));

		glGenBuffers(1, &basis.ibo);
		assert(basis.ibo != 0);
		std::array<glm::uvec3, 16> const indices = {
			// Body: Left
			glm::uvec3(0u, 1u, 2u),
			glm::uvec3(0u, 2u, 3u),
			// Body: Back
			glm::uvec3(4u, 0u, 3u),
			glm::uvec3(4u, 3u, 7u),
			// Body: Bottom
			glm::uvec3(0u, 4u, 5u),
			glm::uvec3(0u, 5u, 1u),
			// Body: Front
			glm::uvec3(1u, 5u, 6u),
			glm::uvec3(1u, 6u, 2u),
			// Body: Top
			glm::uvec3(2u, 6u, 7u),
			glm::uvec3(2u, 7u, 3u),
			// Tip: Left
			glm::uvec3(8u, 9u, 10u),
			glm::uvec3(8u, 10u, 11u),
			// Tip: Back
			glm::uvec3(12u, 8u, 11u),
			// Tip: Bottom
			glm::uvec3(8u, 12u, 9u),
			// Tip: Front
			glm::uvec3(9u, 12u, 10u),
			// Tip: Top
			glm::uvec3(10u, 12u, 11u)
		};
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, basis.ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(), GL_STATIC_DRAW);

		basis.index_count = static_cast<GLsizei>(indices.size() * 3);

		glBindVertexArray(0u);
		glBindBuffer(GL_ARRAY_BUFFER, 0U);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0U);

		basis.shader = bonobo::createProgram("common/basis.vert", "common/basis.frag");
		if (basis.shader == 0u) {
			LogError("Failed to load \"basis.vert\" and \"basis.frag\"");
			return;
		}

		GLint shader_location = glGetUniformLocation(basis.shader, "vertex_model_to_world");
		assert(shader_location >= 0);
		basis.shader_locations.world = shader_location;

		shader_location = glGetUniformLocation(basis.shader, "vertex_world_to_clip");
		assert(shader_location >= 0);
		basis.shader_locations.view_proj = shader_location;

		shader_location = glGetUniformLocation(basis.shader, "thickness_scale");
		assert(shader_location >= 0);
		basis.shader_locations.thickness_scale = shader_location;

		shader_location = glGetUniformLocation(basis.shader, "length_scale");
		assert(shader_location >= 0);
		basis.shader_locations.length_scale = shader_location;

		utils::opengl::debug::nameObject(GL_VERTEX_ARRAY, basis.vao, "Basis VAO");
		utils::opengl::debug::nameObject(GL_BUFFER, basis.vbo, "Basis VBO");
		utils::opengl::debug::nameObject(GL_BUFFER, basis.ibo, "Basis IBO");
	}

	void createDebugTexture()
	{
		const GLsizei debug_texture_width = 16;
		const GLsizei debug_texture_height = 16;
		std::array<std::uint32_t, debug_texture_width* debug_texture_height> debug_texture_content;
		debug_texture_content.fill(0xFFE935DAu);
		glGenTextures(1, &debug_texture_id);
		glBindTexture(GL_TEXTURE_2D, debug_texture_id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, debug_texture_width, debug_texture_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, debug_texture_content.data());
		glBindTexture(GL_TEXTURE_2D, 0u);

		utils::opengl::debug::nameObject(GL_TEXTURE, debug_texture_id, "Debug texture");
	}
}
