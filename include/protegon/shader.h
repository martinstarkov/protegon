#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "protegon/debug.h"
#include "protegon/file.h"
#include "protegon/handle.h"
#include "protegon/type_traits.h"
#include "protegon/matrix4.h"

namespace ptgn {

class Shader;

namespace impl {

std::string_view GetShaderTypeName(std::uint32_t type);

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

	void SetUniform(const std::string& name, const Matrix4<float>& m);
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