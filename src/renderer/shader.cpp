#include "protegon/shader.h"

#include <cassert>
#include <fstream>

#include "gl_loader.h"
#include "protegon/log.h"

namespace ptgn {

namespace impl {

std::string_view GetShaderTypeName(unsigned int type) {
	switch (type) {
		case GL_VERTEX_SHADER:		    return "vertex";
		case GL_FRAGMENT_SHADER:        return "fragment";
		case GL_COMPUTE_SHADER:         return "compute";
		case GL_GEOMETRY_SHADER:        return "geometry";
		case GL_TESS_CONTROL_SHADER:    return "tess control";
		case GL_TESS_EVALUATION_SHADER: return "tess evaluation";
		default:						return "invalid";
	}
}

} // namespace impl

Shader::Shader(const fs::path& vertex_shader_path, const fs::path& fragment_shader_path) {
	std::string vertex_source = FileToString(vertex_shader_path);
	std::string fragment_source = FileToString(fragment_shader_path);
	program_id_ = CompileProgram(vertex_source, fragment_source);
}

void Shader::CreateFromStrings(const std::string& vertex_shader_source, const std::string& fragment_shader_source) {
	program_id_ = CompileProgram(vertex_shader_source, fragment_shader_source);
}

Shader::~Shader() {
	glDeleteProgram(program_id_);
}

unsigned int Shader::CompileShader(unsigned int type, const std::string& source) {
	unsigned int id = glCreateShader(type);
	const char* src = source.c_str();
	glShaderSource(id, 1, &src, NULL);
	glCompileShader(id);

	// Check for shader compilation errors.
	int result{ GL_FALSE };
	glGetShaderiv(id, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		int length;
		glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
		std::vector<GLchar> log(length);
		glGetShaderInfoLog(id, length, &length, &log[0]);
		PrintLine("Failed to compile ", impl::GetShaderTypeName(type), " shader");
		PrintLine(log.data());
		glDeleteShader(id);
		assert(!false); // OPTIONAL: crash on shader compilation fail.
		return 0;
	}
	return id;
}

unsigned int Shader::CompileProgram(const std::string& vertex_source, const std::string& fragment_source) {
	unsigned int program = glCreateProgram();

	unsigned int vertex = CompileShader(GL_VERTEX_SHADER, vertex_source);
	unsigned int fragment = CompileShader(GL_FRAGMENT_SHADER, fragment_source);

	if (vertex && fragment) {
		glAttachShader(program, vertex);
		glAttachShader(program, fragment);
		glLinkProgram(program);
		glValidateProgram(program);

		// Check for shader link errors.
		int linked = GL_FALSE;
		glGetProgramiv(program, GL_LINK_STATUS, &linked);
		if (linked == GL_FALSE) {
			int length = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

			std::vector<GLchar> log(length);
			glGetProgramInfoLog(program, length, &length, &log[0]);

			glDeleteProgram(program);

			glDeleteShader(vertex);
			glDeleteShader(fragment);

			PrintLine("Failed to link shaders to program");
			PrintLine(log.data());
			assert(!false); // OPTIONAL: crash on shader link fail.
			return 0;
		}
	}
	if (vertex) {
		glDeleteShader(vertex);
	}
	if (fragment) {
		glDeleteShader(fragment);
	}
	return program;
}

void Shader::Bind() {
	assert(program_id_ != 0 && "Cannot use shader which has not been initialized correctly");
	glUseProgram(program_id_);
}

void Shader::Unbind() {
	glUseProgram(0);
}

unsigned int Shader::GetProgramId() const {
	return program_id_;
}

int Shader::GetUniformLocation(const std::string& name) const {
	auto it = location_cache_.find(name);
	if (it != location_cache_.end()) {
		return it->second;
	}

	int location = glGetUniformLocation(program_id_, name.c_str());
	// TODO: Consider not adding uniform to cache if it is -1.
	location_cache_[name] = location;
	return location;
}

void Shader::SetUniform(const std::string& name, float v0) {
	int location = GetUniformLocation(name);
	if (location != -1)
		glUniform1f(location, v0);
}

void Shader::SetUniform(const std::string& name, float v0, float v1) {
	int location = GetUniformLocation(name);
	if (location != -1)
		glUniform2f(location, v0, v1);
}

void Shader::SetUniform(const std::string& name, float v0, float v1, float v2) {
	int location = GetUniformLocation(name);
	if (location != -1)
		glUniform3f(location, v0, v1, v2);
}

void Shader::SetUniform(const std::string& name, float v0, float v1, float v2, float v3) {
	int location = GetUniformLocation(name);
	if (location != -1)
		glUniform4f(location, v0, v1, v2, v3);
}

void Shader::SetUniform(const std::string& name, int v0) {
	int location = GetUniformLocation(name);
	if (location != -1)
		glUniform1i(location, v0);
}

void Shader::SetUniform(const std::string& name, int v0, int v1) {
	int location = GetUniformLocation(name);
	if (location != -1)
		glUniform2i(location, v0, v1);
}

void Shader::SetUniform(const std::string& name, int v0, int v1, int v2) {
	int location = GetUniformLocation(name);
	if (location != -1)
		glUniform3i(location, v0, v1, v2);
}

void Shader::SetUniform(const std::string& name, int v0, int v1, int v2, int v3) {
	int location = GetUniformLocation(name);
	if (location != -1)
		glUniform4i(location, v0, v1, v2, v3);
}

void Shader::SetUniform(const std::string& name, bool value) {
	SetUniform(name, static_cast<int>(value));
}

} // namespace ptgn
