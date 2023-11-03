#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <string>
#include <vector>


namespace utils
{

namespace opengl
{

namespace debug
{

bool isSupported();
std::string getStringForType(GLenum type);
std::string getStringForSource(GLenum source);
std::string getStringForSeverity(GLenum severity);

//! \brief Start a new debug group.
//!
//! This will allow tools like RenderDoc or Nsight Graphics, or the debug
//! messages, to group all the commands issued within this newly defined scope
//! under the specified name/ message.
//!
//! The call will be ignored if OpenGL debug facilities are not available.
//!
//! \param [in] message message or name for the new group to create
//! \param [in] id An ID for the current message, which could be used for
//!             filtering some messages out but is currently unused
void beginDebugGroup(std::string const& message, GLuint id = 0u);

//! \brief End the most recently-started debug group.
//!
//! The call will be ignored if OpenGL debug facilities are not available.
void endDebugGroup();

//! \brief Label an OpenGL object with a custon string.
//!
//! This will allow tools like RenderDoc or Nsight Graphics, or the debug
//! messages, to display those strings rather than the object ID, making it
//! easier to quickly find a given object.
//!
//! The call will be ignored if OpenGL debug facilities are not available.
//!
//! \param [in] type the type of the object to name, like GL_BUFFER, GL_TEXTURE
//! \param [in] id the ID of the OpenGL object to name
//! \param [in] label the label to associate to the given object
void nameObject(GLenum type, GLuint id, std::string const& label);

void
#ifdef _WIN32
APIENTRY
#endif
opengl_error_callback( GLenum source, GLenum type, GLuint id
                     , GLenum severity, GLsizei /*length*/
                     , GLchar const* msg, void const* /*data*/
                     );

} // end of namespace debug

namespace shader
{

bool source_and_build_shader(GLuint id, std::string const& source);
GLuint generate_shader(GLenum type, std::string const& source);
bool link_program(GLuint id);
void reload_program(GLuint id, std::vector<GLuint> const& ids, std::vector<std::string> const& sources);
GLuint generate_program(std::vector<GLuint> const& shaders_id);

} // end of namespace shader

namespace fullscreen
{

void init(std::string const& vs_path, std::string const& fs_path, size_t width, size_t height);
void deinit();
void draw();
GLuint get_texture_id();

} // end of namespace fullscreen

} // end of namespace opengl

} // end of namespace utils
