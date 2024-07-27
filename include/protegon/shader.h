#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "protegon/file.h"
#include "protegon/matrix4.h"
#include "utility/debug.h"
#include "utility/handle.h"
#include "utility/type_traits.h"

namespace ptgn {

class Shader;

namespace impl {

std::string_view GetShaderTypeName(std::uint32_t type);

struct ShaderInstance {
	ShaderInstance();
	~ShaderInstance();
	// Location cache should not prevent const calls.
	mutable std::unordered_map<std::string, std::int32_t> location_cache_;
	std::uint32_t id_{ 0 };
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
	Shader()  = default;
	~Shader() = default;

	Shader(const ShaderSource& vertex_shader, const ShaderSource& fragment_shader);
	Shader(const path& vertex_shader_path, const path& fragment_shader_path);

	void WhileBound(const std::function<void()>& func);

	void SetUniform(const std::string& name, const Vector2<float>& v) const;
	void SetUniform(const std::string& name, const Vector3<float>& v) const;
	void SetUniform(const std::string& name, const Vector4<float>& v) const;
	void SetUniform(const std::string& name, const Matrix4<float>& m) const;
	void SetUniform(const std::string& name, float v0) const;
	void SetUniform(const std::string& name, float v0, float v1) const;
	void SetUniform(const std::string& name, float v0, float v1, float v2) const;
	void SetUniform(const std::string& name, float v0, float v1, float v2, float v3) const;
	void SetUniform(const std::string& name, const Vector2<std::int32_t>& v) const;
	void SetUniform(const std::string& name, const Vector3<std::int32_t>& v) const;
	void SetUniform(const std::string& name, const Vector4<std::int32_t>& v) const;
	void SetUniform(const std::string& name, std::int32_t v0) const;
	// Behaves identically to SetUniform(name, std::int32_t).
	void SetUniform(const std::string& name, bool value) const;
	void SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1) const;
	void SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2)
		const;
	void SetUniform(
		const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2, std::int32_t v3
	) const;

	void Bind() const;
	void Unbind() const;

private:
	[[nodiscard]] std::int32_t GetUniformLocation(const std::string& name) const;

	[[nodiscard]] void CompileProgram(
		const std::string& vertex_shader, const std::string& fragment_shader
	);

	// Returns shader id.
	[[nodiscard]] std::uint32_t CompileShader(std::uint32_t type, const std::string& source);
};

} // namespace ptgn