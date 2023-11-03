#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "core/FPSCamera.h" // As it includes OpenGL headers, import it after glad

#include <functional>
#include <string>
#include <vector>
#include <unordered_map>

//! \brief Namespace containing a few helpers for the LUGG computer graphics labs.
namespace bonobo
{
	//! \brief Formalise mapping between an OpenGL VAO attribute binding,
	//!        and the meaning of that attribute.
	enum class shader_bindings : unsigned int{
		vertices = 0u, //!< = 0, value of the binding point for vertices
		normals,       //!< = 1, value of the binding point for normals
		texcoords,     //!< = 2, value of the binding point for texcoords
		tangents,      //!< = 3, value of the binding point for tangents
		binormals      //!< = 4, value of the binding point for binormals
	};

	//! \brief Association of a sampler name used in GLSL to a
	//!        corresponding texture ID.
	using texture_bindings = std::unordered_map<std::string, GLuint>;

	struct material_data {
		glm::vec3 diffuse{ 0.0f };
		glm::vec3 specular{ 0.0f };
		glm::vec3 ambient{ 0.0f };
		glm::vec3 emissive{ 0.0f };
		float shininess{ 0.0f };
		float indexOfRefraction{ 1.0f };
		float opacity{ 1.0f };
	};

	//! \brief Contains the data for a mesh in OpenGL.
	struct mesh_data {
		GLuint vao{0u};                          //!< OpenGL name of the Vertex Array Object
		GLuint bo{0u};                           //!< OpenGL name of the Buffer Object
		GLuint ibo{0u};                          //!< OpenGL name of the Buffer Object for indices
		GLsizei vertices_nb{0};                  //!< number of vertices stored in bo
		GLsizei indices_nb{0};                   //!< number of indices stored in ibo
		texture_bindings bindings{};             //!< texture bindings for this mesh
		material_data material{};                //!< constant values for the material of this mesh
		GLenum drawing_mode{GL_TRIANGLES};       //!< OpenGL drawing mode, i.e. GL_TRIANGLES, GL_LINES, etc.
		std::string name{"un-named mesh"};       //!< Name of the mesh; used for debugging purposes.
	};

	enum class cull_mode_t : unsigned int {
		disabled = 0u,
		back_faces,
		front_faces
	};

	enum class polygon_mode_t : unsigned int {
		fill = 0u,
		line,
		point
	};

	//! \brief Allocate some objects needed by some helper functions.
	void init();

	//! \brief Deallocate objects allocated by the `init()` function.
	void deinit();

	//! \brief Load objects found in an object/scene file, using assimp.
	//!
	//! @param [in] filename of the object/scene file to load.
	//! @return a vector of filled in `mesh_data` structures, one per
	//!         object found in the input file
	std::vector<mesh_data> loadObjects(std::string const& filename);

	//! \brief Creates an OpenGL texture without any content nor parameters.
	//!
	//! @param [in] width width of the texture to create
	//! @param [in] height height of the texture to create
	//! @param [in] target OpenGL texture target to create, i.e.
	//!             GL_TEXTURE_2D & co.
	//! @param [in] internal_format formatting of the texture, i.e. how many
	//!             channels
	//! @param [in] format formatting of the pixel data, i.e. in which
	//!             layout are the channels stored
	//! @param [in] type data type of the pixel data
	//! @param [in] data what to put in the texture
	GLuint createTexture(uint32_t width, uint32_t height,
	                     GLenum target = GL_TEXTURE_2D,
	                     GLint internal_format = GL_RGBA,
	                     GLenum format = GL_RGBA,
	                     GLenum type = GL_UNSIGNED_BYTE,
	                     GLvoid const* data = nullptr);

	//! \brief Load an image into an OpenGL 2D-texture.
	//!
	//! @param [in] filename of the image.
	//! @param [in] generate_mipmap whether or not to generate a mipmap hierarchy
	//! @return the name of the OpenGL 2D-texture
	GLuint loadTexture2D(std::string const& filename,
	                     bool generate_mipmap = true);

	//! \brief Load six images into an OpenGL cubemap-texture.
	//!
	//! @param [in] posx path to the texture on the left of the cubemap
	//! @param [in] negx path to the texture on the right of the cubemap
	//! @param [in] posy path to the texture on the top of the cubemap
	//! @param [in] negy path to the texture on the bottom of the cubemap
	//! @param [in] posz path to the texture on the back of the cubemap
	//! @param [in] negz path to the texture on the front of the cubemap
	//! @param [in] generate_mipmap whether or not to generate a mipmap hierarchy
	//! @return the name of the OpenGL cubemap-texture
	GLuint loadTextureCubeMap(std::string const& posx, std::string const& negx,
                                  std::string const& posy, std::string const& negy,
                                  std::string const& posz, std::string const& negz,
                                  bool generate_mipmap = true);

	//! \brief Create an OpenGL program consisting of a vertex and a
	//!        fragment shader.
	//!
	//! @param [in] vert_shader_source_path of the vertex shader source
	//!             code, relative to the `shaders/` folder
	//! @param [in] frag_shader_source_path of the fragment shader source
	//!             code, relative to the `shaders/` folder
	//! @return the name of the OpenGL shader program
	GLuint createProgram(std::string const& vert_shader_source_path,
	                     std::string const& frag_shader_source_path);

	//! \brief Display the current texture in the specified rectangle.
	//!
	//! @param [in] lower_left the lower left corner of the rectangle
	//!             containing the texture
	//! @param [in] upper_right the upper rigth corner of the rectangle
	//!             containing the texture
	//! @param [in] texture the OpenGL name of the texture to display
	//! @param [in] sampler the OpenGL name of the sampler to use
	//! @param [in] swizzle how to mix in the different channels, for
	//!             example (0, 2, 1, -1) will swap the green and blue
	//!             channels as well as invalidating (setting it to 1) the
	//!             alpha channel
	//! @param [in] window_size the size in pixels of the main window, the
	//!             one relative to which you want to draw this texture
	//! @param [in] linearise whether the given texture should be
	//!             linearised using the provided |nearPlane| and
	//!             |farPlane|.
	//! @param [in] nearPlane the near plane used when linearising depth
	//!             textures; it is ignored if |linearise| is false.
	//! @param [in] farPlane the far plane used when linearising depth
	//!             textures; it is ignored if |linearise| is false.
	void displayTexture(glm::vec2 const& lower_left,
	                    glm::vec2 const& upper_right, GLuint texture,
	                    GLuint sampler, glm::ivec4 const& swizzle,
	                    glm::ivec2 const& window_size, bool linearise = false,
	                    float nearPlane = 0.0f, float farPlane = 0.0f);

	//! \brief Create an OpenGL FrameBuffer Object using the specified
	//!        attachments.
	//!
	//! @param [in] color_attachments a vector of all the texture to bind
	//!             as color attachment, i.e. not as depth texture
	//! @param [in] depth_attachment a texture, if any, to use as depth
	//!             attachment
	//! @return the name of the OpenGL FBO
	GLuint createFBO(std::vector<GLuint> const& color_attachments,
	                 GLuint depth_attachment = 0u);

	//! \brief Create an OpenGL sampler and set it up.
	//!
	//! @param [in] setup a lambda function to parameterise the sampler
	//! @return the name of the OpenGL sampler
	GLuint createSampler(std::function<void (GLuint)> const& setup);

	//! \brief Draw full screen.
	void drawFullscreen();

	//! \brief Retrieve the ID of a small placeholder texture.
	GLuint getDebugTextureID();

	//! \brief Render a right-hand orthonormal basis.
	//!
	//! @param [in] thickness_scale By how much to scale the thickness of the axes
	//! @param [in] length_scale By how much to scale the length of the axes
	//! @param [in] view_projection Matrix transforming from world-space to clip-space
	//! @param [in] world Matrix transforming from model-space to
	//!             world-space
	void renderBasis(float thickness_scale, float length_scale, glm::mat4 const& view_projection, glm::mat4 const& world = glm::mat4(1.0f));

	//! \brief Add a combo box to the current ImGUI window, to choose a
	//!        cull mode.
	//!
	//! @param [in] label Text to be displayed near the combo box.
	//! @param [inout] cull_mode The currently selected cull mode,
	//!                which will be modified to contain the newly selected
	//!                one.
	//! @return whether the selection was changed
	bool uiSelectCullMode(std::string const& label, enum cull_mode_t& cull_mode) noexcept;

	//! \brief Enable or disable culling, and call glCullFace with the
	//!        specified cull mode.
	void changeCullMode(enum cull_mode_t const cull_mode) noexcept;

	//! \brief Add a combo box to the current ImGUI window, to choose a
	//!        polygon mode.
	//!
	//! @param [in] label Text to be displayed near the combo box.
	//! @param [inout] polygon_mode The currently selected polygon mode,
	//!                which will be modified to contain the newly selected
	//!                one.
	//! @return whether the selection was changed
	bool uiSelectPolygonMode(std::string const& label, enum polygon_mode_t& polygon_mode) noexcept;

	//! \brief Call glPolygonMode for both front and back faces, with the
	//!        specified polygon mode.
	void changePolygonMode(enum polygon_mode_t const polygon_mode) noexcept;
}
