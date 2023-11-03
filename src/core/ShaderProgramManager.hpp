#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <cstdint>

enum class ShaderType : std::uint32_t {
	vertex = GL_VERTEX_SHADER,
	tess_eval = GL_TESS_EVALUATION_SHADER,
	tess_ctrl = GL_TESS_CONTROL_SHADER,
	geometry = GL_GEOMETRY_SHADER,
	fragment = GL_FRAGMENT_SHADER,
	compute = GL_COMPUTE_SHADER
};

class ShaderProgramManager
{
public:
	using ProgramData = std::map<ShaderType, std::string>;
	struct SelectedProgram {
		bool was_selection_changed = false;
		GLuint const* program = nullptr;
		char const* name = nullptr;
	};
	~ShaderProgramManager();
	void CreateAndRegisterProgram(char const* const program_name, ProgramData const& program_data, GLuint& program);
	void CreateAndRegisterComputeProgram(char const* const program_name, std::string const& filename, GLuint& program);
	bool ReloadAllPrograms();
	SelectedProgram SelectProgram(std::string const& label, std::int32_t& program_index);

private:
	void ProcessProgram(std::size_t program_index);
	using ProgramEntry = std::pair<GLuint&, ProgramData>;
	std::vector<ProgramEntry> program_entries;
	std::vector<char const*> program_names;
};
