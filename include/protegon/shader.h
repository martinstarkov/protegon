#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "file.h"

namespace ptgn {

namespace impl {

std::string_view GetShaderTypeName(unsigned int type);

} // namespace impl

class Shader {
public:
	Shader(const fs::path& vertex_shader, const fs::path& fragment_shader);
	~Shader();

	void SetUniform(const std::string& name, float v0);
	void SetUniform(const std::string& name, float v0, float v1);
	void SetUniform(const std::string& name, float v0, float v1, float v2);
	void SetUniform(const std::string& name, float v0, float v1, float v2, float v3);
	void SetUniform(const std::string& name, int v0);
	// Behaves identically to SetUniform(name, int).
	void SetUniform(const std::string& name, bool value);
	void SetUniform(const std::string& name, int v0, int v1);
	void SetUniform(const std::string& name, int v0, int v1, int v2);
	void SetUniform(const std::string& name, int v0, int v1, int v2, int v3);

	int GetUniformLocation(const std::string& name) const;

	void Bind();
	void Unbind();

	unsigned int GetProgramId() const;
private:
	// Cache should not prevent const calls.
	mutable std::unordered_map<std::string, int> location_cache_;


	// Returns program id.
	unsigned int CompileProgram(const fs::path& vertex_shader, const fs::path& fragment_shader);
	// Returns shader id.
	unsigned int CompileShader(unsigned int type, const std::string& source);

	unsigned int program_id_{ 0 };
};

} // namespace ptgn