#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <cstdint>
#include <memory>

#include "utility/debug.h"
#include "type_traits.h"
#include "file.h"
#include "handle.h"

namespace ptgn {

namespace impl {

std::string_view GetShaderTypeName(std::uint32_t type);

enum class GLSLType : std::uint32_t {
	None		  = 0,
	Byte		  = 0x1400, // GL_BYTE
	UnsignedByte  = 0x1401, // GL_UNSIGNED_BYTE
	Short		  = 0x1402, // GL_SHORT
	UnsignedShort = 0x1403, // GL_UNSIGNED_SHORT
	Int			  = 0x1404, // GL_INT
	UnsignedInt	  = 0x1405, // GL_UNSIGNED_INT
	Float		  = 0x1406, // GL_FLOAT
	Double		  = 0x140A, // GL_DOUBLE
};

template <typename T>
[[nodiscard]] GLSLType GetType() {
	static_assert(type_traits::is_one_of_v<T,
		std::float_t, std::double_t, std::int32_t, std::uint32_t,
		std::int16_t, std::uint16_t, std::int8_t, std::uint8_t, bool>, "Cannot retrieve type which is not supported by OpenGL");

	     if constexpr (std::is_same_v<T, std::float_t>)								return GLSLType::Float;
	else if constexpr (std::is_same_v<T, std::double_t>)							return GLSLType::Double;
	else if constexpr (std::is_same_v<T, std::int32_t>)								return GLSLType::Int;
	else if constexpr (std::is_same_v<T, std::uint32_t>)							return GLSLType::UnsignedInt;
	else if constexpr (std::is_same_v<T, std::int16_t>)								return GLSLType::Short;
	else if constexpr (std::is_same_v<T, std::uint16_t>)							return GLSLType::UnsignedShort;
	else if constexpr (std::is_same_v<T, std::int8_t> || std::is_same_v<T, bool>)   return GLSLType::Byte;
	else if constexpr (std::is_same_v<T, std::uint8_t>)								return GLSLType::UnsignedByte;
	PTGN_ASSERT(false, "Failed to retrieve type of buffer element");
	return GLSLType::None;
}

} // namespace impl

/*
enum class ShaderDataType : std::uint64_t {
	// To encode information in:
	// std::uint64_t encoded = 
	// (static_cast<std::uint64_t>(hidden_size) << 48) | 
	// ((static_cast<std::uint64_t>(hidden_count) << 32) | hidden_type);
	// To extract information out use the ShaderDataInfo class:
	// ShaderDataInfo info{ shader_data_type };
	// std::uint16_t size  = info.size;
	// std::uint16_t count = info.count;
	// std::uint32_t type = info.type;

	none    = 0,
	float_  = (static_cast<std::uint64_t>(sizeof(float))        << 48) | ((static_cast<std::uint64_t>(1) << 32) | impl::TYPE_FLOAT),
	vec2    = (static_cast<std::uint64_t>(sizeof(float))        << 48) | ((static_cast<std::uint64_t>(2) << 32) | impl::TYPE_FLOAT),
	vec3    = (static_cast<std::uint64_t>(sizeof(float))        << 48) | ((static_cast<std::uint64_t>(3) << 32) | impl::TYPE_FLOAT),
	vec4    = (static_cast<std::uint64_t>(sizeof(float))        << 48) | ((static_cast<std::uint64_t>(4) << 32) | impl::TYPE_FLOAT),
	int_    = (static_cast<std::uint64_t>(sizeof(int))          << 48) | ((static_cast<std::uint64_t>(1) << 32) | impl::TYPE_INT),
	ivec2   = (static_cast<std::uint64_t>(sizeof(int))          << 48) | ((static_cast<std::uint64_t>(2) << 32) | impl::TYPE_INT),
	ivec3   = (static_cast<std::uint64_t>(sizeof(int))          << 48) | ((static_cast<std::uint64_t>(3) << 32) | impl::TYPE_INT),
	ivec4   = (static_cast<std::uint64_t>(sizeof(int))          << 48) | ((static_cast<std::uint64_t>(4) << 32) | impl::TYPE_INT),
	uint_   = (static_cast<std::uint64_t>(sizeof(unsigned int)) << 48) | ((static_cast<std::uint64_t>(1) << 32) | impl::TYPE_UNSIGNED_INT),
	uvec2   = (static_cast<std::uint64_t>(sizeof(unsigned int)) << 48) | ((static_cast<std::uint64_t>(2) << 32) | impl::TYPE_UNSIGNED_INT),
	uvec3   = (static_cast<std::uint64_t>(sizeof(unsigned int)) << 48) | ((static_cast<std::uint64_t>(3) << 32) | impl::TYPE_UNSIGNED_INT),
	uvec4   = (static_cast<std::uint64_t>(sizeof(unsigned int)) << 48) | ((static_cast<std::uint64_t>(4) << 32) | impl::TYPE_UNSIGNED_INT),
	double_ = (static_cast<std::uint64_t>(sizeof(double))       << 48) | ((static_cast<std::uint64_t>(1) << 32) | impl::TYPE_DOUBLE),
	dvec2   = (static_cast<std::uint64_t>(sizeof(double))       << 48) | ((static_cast<std::uint64_t>(2) << 32) | impl::TYPE_DOUBLE),
	dvec3   = (static_cast<std::uint64_t>(sizeof(double))       << 48) | ((static_cast<std::uint64_t>(3) << 32) | impl::TYPE_DOUBLE),
	dvec4   = (static_cast<std::uint64_t>(sizeof(double))       << 48) | ((static_cast<std::uint64_t>(4) << 32) | impl::TYPE_DOUBLE),
	bool_   = (static_cast<std::uint64_t>(sizeof(bool))         << 48) | ((static_cast<std::uint64_t>(1) << 32) | impl::TYPE_BYTE),
	bvec2   = (static_cast<std::uint64_t>(sizeof(bool))         << 48) | ((static_cast<std::uint64_t>(2) << 32) | impl::TYPE_BYTE),
	bvec3   = (static_cast<std::uint64_t>(sizeof(bool))         << 48) | ((static_cast<std::uint64_t>(3) << 32) | impl::TYPE_BYTE),
	bvec4   = (static_cast<std::uint64_t>(sizeof(bool))         << 48) | ((static_cast<std::uint64_t>(4) << 32) | impl::TYPE_BYTE)
};

struct ShaderDataInfo {
	ShaderDataInfo(ShaderDataType encoded);
	ShaderDataInfo(std::uint64_t encoded);
	std::uint16_t size{ 0 };    // Size of an individual buffer element.
	std::uint16_t count{ 0 };   // Number of buffer elements.
	impl::GLEnumType type{ 0 }; // Type of buffer element (e.g. GL_FLOAT).
};
*/

class Shader;

namespace impl {

using Id = std::uint32_t;
	
class ShaderInstance {
public:
	ShaderInstance() = default;
	~ShaderInstance();
private:
	ShaderInstance(Id program_id);
private:
	friend class Shader;
	// Cache should not prevent const calls.
	mutable std::unordered_map<std::string, std::int32_t> location_cache_;
	Id program_id_{ 0 };
};

} // namespace impl

// Wrapper for distinguishing between Shader from path construction and Shader from source construction.
class ShaderSource {
public:
	ShaderSource() = delete;
	// Explicit prevents conflict with Shader path construction.
	explicit ShaderSource(const std::string& source) : source_{ source } {}
	~ShaderSource() = default;
private:
	friend class Shader;
	const std::string source_;
};

class Shader : public Handle<impl::ShaderInstance> {
public:
	Shader() = default;

	Shader(const ShaderSource& vertex_shader, const ShaderSource& fragment_shader);
	Shader(const path& vertex_shader_path, const path& fragment_shader_path);

	void SetUniform(const std::string& name, float v0);
	void SetUniform(const std::string& name, float v0, float v1);
	void SetUniform(const std::string& name, float v0, float v1, float v2);
	void SetUniform(const std::string& name, float v0, float v1, float v2, float v3);
	void SetUniform(const std::string& name, std::int32_t v0);
	// Behaves identically to SetUniform(name, std::int32_t).
	void SetUniform(const std::string& name, bool value);
	void SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1);
	void SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2);
	void SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2, std::int32_t v3);

	[[nodiscard]] std::int32_t GetUniformLocation(const std::string& name) const;

	void Bind();
	void Unbind();

private:
	void Create(const std::string& vertex_shader_source, const std::string& fragment_shader_source);
	[[nodiscard]] impl::Id GetProgramId() const;
	// Returns program id.
	[[nodiscard]] impl::Id CompileProgram(const std::string& vertex_shader, const std::string& fragment_shader);
	// Returns shader id.
	[[nodiscard]] std::uint32_t CompileShader(std::uint32_t type, const std::string& source);
};

} // namespace ptgn