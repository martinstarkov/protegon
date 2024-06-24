#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "file.h"
#include "handle.h"
#include "protegon/debug.h"
#include "type_traits.h"

namespace ptgn {

class Shader;

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
	static_assert(
		type_traits::is_one_of_v<
			T, float, double, std::int32_t, std::uint32_t, std::int16_t,
			std::uint16_t, std::int8_t, std::uint8_t, bool>,
		"Cannot retrieve type which is not supported by OpenGL"
	);

	if constexpr (std::is_same_v<T, float>) {
		return GLSLType::Float;
	} else if constexpr (std::is_same_v<T, double>) {
		return GLSLType::Double;
	} else if constexpr (std::is_same_v<T, std::int32_t>) {
		return GLSLType::Int;
	} else if constexpr (std::is_same_v<T, std::uint32_t>) {
		return GLSLType::UnsignedInt;
	} else if constexpr (std::is_same_v<T, std::int16_t>) {
		return GLSLType::Short;
	} else if constexpr (std::is_same_v<T, std::uint16_t>) {
		return GLSLType::UnsignedShort;
	} else if constexpr (std::is_same_v<T, std::int8_t> || std::is_same_v<T, bool>) {
		return GLSLType::Byte;
	} else if constexpr (std::is_same_v<T, std::uint8_t>) {
		return GLSLType::UnsignedByte;
	}
}

} // namespace impl

namespace impl {

using Id = std::uint32_t;

struct ShaderInstance {
	ShaderInstance() = default;
	~ShaderInstance();
	ShaderInstance(Id program_id);
	// Cache should not prevent const calls.
	mutable std::unordered_map<std::string, std::int32_t> location_cache_;
	Id program_id_{ 0 };
};

} // namespace impl

// Wrapper for distinguishing between Shader from path construction and Shader
// from source construction.
struct ShaderSource {
	ShaderSource() = delete;
	// Explicit prevents conflict with Shader path construction.
	explicit ShaderSource(const std::string& source) : source_{ source } {}
	~ShaderSource() = default;
	const std::string source_;
};

class Shader : public Handle<impl::ShaderInstance> {
public:
	Shader() = default;

	Shader(const ShaderSource& vertex_shader, const ShaderSource& fragment_shader);
	Shader(const path& vertex_shader_path, const path& fragment_shader_path);

	void WhileBound(std::function<void()> func);

	void SetUniform(const std::string& name, float v0);
	void SetUniform(const std::string& name, float v0, float v1);
	void SetUniform(const std::string& name, float v0, float v1, float v2);
	void SetUniform(const std::string& name, float v0, float v1, float v2, float v3);
	void SetUniform(const std::string& name, std::int32_t v0);
	// Behaves identically to SetUniform(name, std::int32_t).
	void SetUniform(const std::string& name, bool value);
	void SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1);
	void SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2);
	void SetUniform(
		const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2, std::int32_t v3
	);

	[[nodiscard]] std::int32_t GetUniformLocation(const std::string& name) const;

	void Bind() const;
	void Unbind() const;

private:
	void Create(const std::string& vertex_shader_source, const std::string& fragment_shader_source);
	[[nodiscard]] impl::Id GetProgramId() const;
	// Returns program id.
	[[nodiscard]] impl::Id CompileProgram(
		const std::string& vertex_shader, const std::string& fragment_shader
	);
	// Returns shader id.
	[[nodiscard]] std::uint32_t CompileShader(std::uint32_t type, const std::string& source);
};

} // namespace ptgn