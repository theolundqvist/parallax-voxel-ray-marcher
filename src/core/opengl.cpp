#include "Log.h"
#include "opengl.hpp"
#include "various.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>


namespace utils
{

namespace opengl
{

namespace debug
{

bool isSupported()
{
	return GLAD_GL_VERSION_4_3 || GLAD_GL_KHR_debug;
}

std::string
getStringForType( GLenum type )
{
	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR:
		return"Error";
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
		return "Deprecated Behavior";
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
		return "Undefined Behavior";
	case GL_DEBUG_TYPE_PORTABILITY:
		return "Portability Issue";
	case GL_DEBUG_TYPE_PERFORMANCE:
		return "Performance Issue";
	case GL_DEBUG_TYPE_MARKER:
		return "Stream Annotation";
	case GL_DEBUG_TYPE_PUSH_GROUP:
		return "Push group";
	case GL_DEBUG_TYPE_POP_GROUP:
		return "Pop group";
	case GL_DEBUG_TYPE_OTHER:
		return "Other";
	default:
		assert(false && "Unknown OpenGL Debug Type");
		return "";
	}
}

std::string
getStringForSource( GLenum source )
{
	switch (source)
	{
	case GL_DEBUG_SOURCE_API:
		return "API";
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
		return "Window System";
	case GL_DEBUG_SOURCE_SHADER_COMPILER:
		return "Shader Compiler";
	case GL_DEBUG_SOURCE_THIRD_PARTY:
		return "Third Party";
	case GL_DEBUG_SOURCE_APPLICATION:
		return "Application";
	case GL_DEBUG_SOURCE_OTHER:
		return "Other";
	default:
		assert(false && "Unknown OpenGL Debug Source");
		return "";
	}
}

std::string
getStringForSeverity( GLenum severity )
{
	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH:
		return "High";
	case GL_DEBUG_SEVERITY_MEDIUM:
		return "Medium";
	case GL_DEBUG_SEVERITY_LOW:
		return "Low";
	case GL_DEBUG_SEVERITY_NOTIFICATION:
		return "Notification";
	default:
		assert(false && "Unknown OpenGL Debug Severity");
		return("");
	}
}
void
beginDebugGroup(std::string const& message, GLuint id)
{
	if (!isSupported())
		return;

	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, id, static_cast<GLsizei>(message.size()), message.data());
}

void
endDebugGroup()
{
	if (!isSupported())
		return;

	glPopDebugGroup();
}

void
nameObject(GLenum type, GLuint id, std::string const& label)
{
	if (!isSupported())
		return;

	glObjectLabel(type, id, static_cast<GLsizei>(label.size()), label.data());
}

void
#ifdef _WIN32
APIENTRY
#endif
opengl_error_callback( GLenum source, GLenum type, GLuint id, GLenum severity
                     , GLsizei /*length*/, GLchar const* msg
                     , void const* /*data*/
                     )
{
	if (type == GL_DEBUG_TYPE_PUSH_GROUP) {
		LogInfo("%s\n{", msg);
		return;
	}
	else if (type == GL_DEBUG_TYPE_POP_GROUP) {
		LogInfo("}");
		return;
	}

	std::ostringstream oss;
	oss << "[id: " << id << "] of type " << getStringForType(type)
	    << ", from " << getStringForSource(source) << ":" << std::endl;
	oss << "\t" << msg << std::endl;

	auto const s_msg = oss.str();
	auto const c_msg = s_msg.c_str();
	switch (severity) {
	case GL_DEBUG_SEVERITY_NOTIFICATION:
	case GL_DEBUG_SEVERITY_LOW: // fallthrough
		LogInfo(c_msg);
		break;
	case GL_DEBUG_SEVERITY_MEDIUM:
		LogWarning(c_msg);
		break;
	case GL_DEBUG_SEVERITY_HIGH:
		LogError(c_msg);
		break;
	}
}

} // end of namespace debug

namespace shader
{

bool
source_and_build_shader(GLuint id, std::string const& source)
{
	assert(id > 0u && !source.empty());

	GLchar const* char_source = source.c_str();
	glShaderSource(id, 1, &char_source, NULL);

	glCompileShader(id);
	GLint state = GLint(0);
	glGetShaderiv(id, GL_COMPILE_STATUS, &state);
	auto const wasCompilationSuccessful = state != GL_FALSE;

	GLint log_length = GLint(0);
	glGetShaderiv(id, GL_INFO_LOG_LENGTH, &log_length);

	if (log_length > 0) {
		std::unique_ptr<GLchar[]> log = std::make_unique<GLchar[]>(static_cast<size_t>(log_length));
		glGetShaderInfoLog(id, log_length, NULL, log.get());

		std::ostringstream oss;
		oss << "Shader compiling log:" << std::endl
		    << log.get() << std::endl;
		auto const s_msg = oss.str();
		auto const c_msg = s_msg.c_str();

		if (wasCompilationSuccessful)
			LogInfo("%s", c_msg);
		else
			LogError("%s", c_msg);
	} else if (!wasCompilationSuccessful) {
		LogError("Shader failed to compile but no log available.");
	}

	return wasCompilationSuccessful;
}

GLuint
generate_shader(GLenum type, std::string const& source)
{
	GLuint id = glCreateShader(type);

	auto const success = source_and_build_shader(id, source);
	if (success) {
		return id;
	} else {
		glDeleteShader(id);
		return 0u;
	}
}

bool
link_program(GLuint id)
{
	glLinkProgram(id);
	GLint state = GLint(0);
	glGetProgramiv(id, GL_LINK_STATUS, &state);
	auto const wasLinkingSuccessful = state != GL_FALSE;

	GLint log_length = GLint(0);
	glGetProgramiv(id, GL_INFO_LOG_LENGTH, &log_length);

	if (log_length > 0) {
		std::unique_ptr<GLchar[]> log = std::make_unique<GLchar[]>(static_cast<size_t>(log_length));
		glGetProgramInfoLog(id, log_length, NULL, log.get());

		std::ostringstream oss;
		oss << "Program linking log:" << std::endl
			<< log.get() << std::endl;
		auto const s_msg = oss.str();
		auto const c_msg = s_msg.c_str();

		if (wasLinkingSuccessful)
			LogInfo("%s", c_msg);
		else
			LogError("%s", c_msg);
	} else if (!wasLinkingSuccessful) {
		LogError("Program failed to link but no log available.");
	}

	return wasLinkingSuccessful;
}

void
reload_program(GLuint id, std::vector<GLuint> const& ids, std::vector<std::string> const& sources)
{
	for (unsigned int i = 0u; i < ids.size(); ++i)
		source_and_build_shader(ids[i], sources[i]);

	link_program(id);
}

GLuint
generate_program(std::vector<GLuint> const& shaders_id)
{
	GLuint id = glCreateProgram();

	for (auto shader_id : shaders_id)
		glAttachShader(id, shader_id);

	auto const success = link_program(id);
	if (success) {
		return id;
	} else {
		glDeleteProgram(id);
		return 0u;
	}
}

} // end of namespace shader

namespace fullscreen
{

static auto vao_id = GLuint(0u);
static auto vbo_id = GLuint(0u);
static auto program_id = GLuint(0u);
static auto texture_id = GLuint(0u);

void
init(std::string const& vs_path, std::string const& fs_path, size_t width, size_t height)
{
	assert(vao_id == 0u && vbo_id == 0u && program_id == 0u && texture_id == 0u);

	glGenVertexArrays(1, &vao_id);
	assert(vao_id != 0u);
	glBindVertexArray(vao_id);

	glGenBuffers(1, &vbo_id);
	assert(vbo_id != 0u);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);

	GLfloat const vertices[4 * 2] =
	{
		-1.0f, -1.0f,
		 1.0f ,-1.0f,
		-1.0f,  1.0f,
		 1.0f,  1.0f
	};
	glBufferData(GL_ARRAY_BUFFER, 4 * 2 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);

	auto const vs = shader::generate_shader(GL_VERTEX_SHADER,utils::slurp_file(vs_path));
	auto const fs = shader::generate_shader(GL_FRAGMENT_SHADER,utils::slurp_file(fs_path));
	program_id = shader::generate_program({ vs, fs });
	glDeleteShader(vs);
	glDeleteShader(fs);

	GLint const location = glGetAttribLocation(program_id, "vertex");
	assert(location >= 0);
	glVertexAttribPointer(static_cast<GLuint>(location), 2, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<GLvoid const*>(0x0));
	glEnableVertexAttribArray(static_cast<GLuint>(location));

	glUseProgram(program_id);

	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &texture_id);
	assert(texture_id != 0u);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA, GL_FLOAT, nullptr);
}

void
deinit()
{
	assert(vao_id != 0u && vbo_id != 0u && program_id != 0u && texture_id != 0u);

	GLint param = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &param);
	if (static_cast<GLuint>(param) == texture_id)
		glBindTexture(GL_TEXTURE_2D, 0u);
	glDeleteTextures(1, &texture_id);
	texture_id = 0u;

	GLint const location = glGetAttribLocation(program_id, "vertex");
	assert(location >= 0);
	glDisableVertexAttribArray(static_cast<GLuint>(location));

	glGetIntegerv(GL_CURRENT_PROGRAM, &param);
	if (static_cast<GLuint>(param) == program_id)
		glUseProgram(0u);
	glDeleteProgram(program_id);
	program_id = 0u;

	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &param);
	if (static_cast<GLuint>(param) == vbo_id)
		glBindBuffer(GL_ARRAY_BUFFER, 0u);
	glDeleteBuffers(1, &vbo_id);
	vbo_id = 0u;

	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &param);
	if (static_cast<GLuint>(param) == vao_id)
		glBindVertexArray(0u);
	glDeleteVertexArrays(1, &vao_id);
	vao_id = 0u;
}

GLuint
get_texture_id()
{
	return texture_id;
}

void
draw()
{
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

} // end of namespace fullscreen

} // end of namespace opengl

} // end of namespace utils
