#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <cstdint>

#include "file.h"

namespace ptgn {


namespace impl {

std::string_view GetShaderTypeName(unsigned int type);

} // namespace impl

enum class ShaderDataType : std::uint64_t {
	// To encode information in:
	// std::uint64_t encoded = (static_cast<std::uint64_t>(size) << 32) | count;
	// To extract information out:
	// std::uint32_t size  = encoded >> 32;		   // size of each element
	// std::uint32_t count = encoded & 0xFFFFFFFF; // number of elements	 
	// Alternatively use the ShaderDataInfo class:
	// ShaderDataInfo info{ shader_data_type };
	// std::uint32_t size  = info.size;
	// std::uint32_t count = info.count;
	none    = 0,
	float_  = (sizeof(int)          << 32) | 1,
	int_    = (sizeof(unsigned int) << 32) | 1,
	uint_   = (sizeof(float)        << 32) | 1,
	double_ = (sizeof(double)       << 32) | 1,
	bool_   = (sizeof(bool)         << 32) | 1,
	vec2    = (sizeof(int)          << 32) | 2,
	ivec2   = (sizeof(unsigned int) << 32) | 2,
	uvec2   = (sizeof(float)        << 32) | 2,
	dvec2   = (sizeof(double)       << 32) | 2,
	bvec2   = (sizeof(bool)         << 32) | 2,
	vec3    = (sizeof(int)          << 32) | 3,
	ivec3   = (sizeof(unsigned int) << 32) | 3,
	uvec3   = (sizeof(float)        << 32) | 3,
	dvec3   = (sizeof(double)       << 32) | 3,
	bvec3   = (sizeof(bool)         << 32) | 3,
	vec4    = (sizeof(int)          << 32) | 4,
	ivec4   = (sizeof(unsigned int) << 32) | 4,
	uvec4   = (sizeof(float)        << 32) | 4,
	dvec4   = (sizeof(double)       << 32) | 4,
	bvec4   = (sizeof(bool)         << 32) | 4
};

struct ShaderDataInfo {
	ShaderDataInfo(ShaderDataType encoded) : ShaderDataInfo{ static_cast<std::uint64_t>(encoded) } {}
	ShaderDataInfo(std::uint64_t encoded) :
		size{ encoded >> 32 }, count{ encoded & 0xFFFFFFFF } {}
	std::uint32_t size{ 0 };
	std::uint32_t count{ 0 };
};

class Shader {
public:
	Shader() = default;
	Shader(const fs::path& vertex_shader_path, const fs::path& fragment_shader_path);
	void CreateFromStrings(const std::string& vertex_shader_source, const std::string& fragment_shader_source);
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
	unsigned int CompileProgram(const std::string& vertex_shader, const std::string& fragment_shader);
	// Returns shader id.
	unsigned int CompileShader(unsigned int type, const std::string& source);

	unsigned int program_id_{ 0 };
};

} // namespace ptgn