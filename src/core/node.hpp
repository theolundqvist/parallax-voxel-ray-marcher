#pragma once

#include "helpers.hpp"
#include "TRSTransform.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <functional>
#include <string>
#include <tuple>
#include <vector>

//! \brief Represents a node of a scene graph
class Node
{
public:
	//! \brief Render this node.
	//!
	//! @param [in] view_projection Matrix transforming from world-space to clip-space
	//! @param [in] parent_transform Matrix transforming from parent-space to
	//!             world-space
	void render(glm::mat4 const& view_projection,
	            glm::mat4 const& parent_transform = glm::mat4(1.0f)) const;

	//! \brief Render this node with a specific shader program.
	//!
	//! Note that the internal transform of this node is **not** used
	//! during the rendering, only the |view_projection| and |world|
	//! matrices are.
	//!
	//! @param [in] view_projection Matrix transforming from world-space to clip-space
	//! @param [in] world Matrix transforming from model-space to
	//!             world-space
	//! @param [in] program OpenGL shader program to use
	//! @param [in] set_uniforms function that will take as argument an
	//!             OpenGL shader program, and will setup that program's
	//!             uniforms
	void render(glm::mat4 const& view_projection, glm::mat4 const& world,
	            GLuint program,
	            std::function<void (GLuint)> const& set_uniforms = [](GLuint /*programID*/){}) const;

	void renderCoool(glm::mat4 const& transform, glm::mat4 const& view_projection);

	//! \brief Set the geometry of this node.
	//!
	//! It will overwrite any constants provided by an earlier call to
	//! |set_material_constants()|.
	//!
	//! A node without any geometry will not render itself, but its
	//! children will be rendered if they have any geometry.
	//!
	//! @param [in] shape OpenGL data to use as geometry
	void set_geometry(bonobo::mesh_data const& shape);

	//! \brief Set the material constants of this node.
	//!
	//! It will overwrite any constants provided by the geometry.
	//!
	//! A node without any geometry will not render itself, but its
	//! children will be rendered if they have any geometry.
	//!
	//! @param [in] constants Material constants to be made available during rendering
	void set_material_constants(bonobo::material_data const& constants);

	//! \brief Get the number of indices to use.
	//!
	//! @return how many indices to use when rendering
	size_t get_indices_nb() const;

	//! \brief Set the number of indices to use.
	//!
	//! @param [in] indices_nb how many indices to use when rendering
	void set_indices_nb(size_t const& indices_nb);

	//! \brief Set the program of this node.
	//!
	//! A node without a program will not render itself, but its children
	//! will be rendered if they have one.
	//!
	//! @param [in] program pointer to the program OpenGL shader program to
	//!             use; the pointer should not be null.
	//! @param [in] set_uniforms function that will take as argument an
	//!             OpenGL shader program, and will setup that program's
	//!             uniforms
	void set_program(GLuint const* const program,
	                 std::function<void (GLuint)> const& set_uniforms = [](GLuint /*programID*/){});

	//! \brief Set the name of this node.
	//!
	//! This name will be used when pushing debug groups to scope OpenGL
	//! commands and help when debugging or profiling the application using
	//! third-party applications.
	//!
	//! @param [in] name the name used when creating the debug group during
	//!             rendering; it will automatically be prefixed by "Render ".
	void set_name(std::string const& name);

	//! \brief Add a texture to this node.
	//!
	//! @param [in] name the variable name used by the attached OpenGL
	//!                  shader program; in assignment 1, this will be
	//!                  `diffuse_texture`
	//! @param [in] tex_id the name of an OpenGL 2D-texture
	//! @param [in] type the type of texture, i.e. GL_TEXTURE_2D,
	//!                  GL_TEXTURE_CUBE_MAP, etc.
	void add_texture(std::string const& name, GLuint tex_id, GLenum type);

	void set_texture(std::string const& name, GLuint tex_id, GLenum type);

	//! \brief Add a child to this node.
	//!
	//! @param [in] child pointer to the child to add; the pointer has to
	//!             be non-null
	void add_child(Node const* child);

	//! \brief Return the number of children to this node.
	//!
	//! @return the number of children
	size_t get_children_nb() const;

	//! \brief Return the ith child.
	//!
	//! @param [in] index the index of the child to return; index should be
	//!             strictly less than the number of children
	//! @return a pointer to the desired child
	Node const* get_child(size_t index) const;

	//! \brief Return this node transformation matrix.
	//!
	//! @return the composition of the rotation, scaling and translation
	//!         transformations; this is the model matrix of this node
	TRSTransformf const& get_transform() const;
	TRSTransformf& get_transform();

private:
	// Geometry data
	GLuint _vao{ 0u };
	GLsizei _vertices_nb{ 0u };
	GLsizei _indices_nb{ 0u };
	GLenum _drawing_mode{ GL_TRIANGLES };
	bool _has_indices{ false };

	// Program data
	GLuint const* _program{ nullptr };
	std::function<void (GLuint)> _set_uniforms;

	// Material data
	std::vector<std::tuple<std::string, GLuint, GLenum>> _textures;
	bonobo::material_data _constants;

	// Transformation data
	TRSTransformf _transform;

	// Children data
	std::vector<Node const*> _children;

	// Debug data
	std::string _name{"Render un-named node"};
};
