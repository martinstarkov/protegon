#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "core/manager.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "utility/debug.h"
#include "utility/file.h"
#include "utility/handle.h"

// clang-format off
#define PTGN_SHADER_STRINGIFY_MACRO(x) PTGN_STRINGIFY_MACRO(x)

#ifdef __EMSCRIPTEN__
#define PTGN_SHADER_PATH(file) PTGN_SHADER_STRINGIFY_MACRO(PTGN_EXPAND_MACRO(resources/shader/es/)PTGN_EXPAND_MACRO(file))
#else
#define PTGN_SHADER_PATH(file) PTGN_SHADER_STRINGIFY_MACRO(PTGN_EXPAND_MACRO(resources/shader/core/)PTGN_EXPAND_MACRO(file))
#endif
// clang-format on

namespace ptgn {

class Shader;

namespace impl {

class Game;
class RendererData;

[[nodiscard]] std::string_view GetShaderTypeName(std::uint32_t type);

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
	ShaderSource() = default;

	// Explicit prevents conflict with Shader path construction.
	explicit ShaderSource(const std::string& source) : source_{ source } {}

	~ShaderSource() = default;
	std::string source_;
};

class Shader : public Handle<impl::ShaderInstance> {
public:
	using Handle::Handle;

	Shader()		   = default;
	~Shader() override = default;

	Shader(const ShaderSource& vertex_shader, const ShaderSource& fragment_shader);
	Shader(const path& vertex_shader_path, const path& fragment_shader_path);

	void SetUniform(const std::string& name, const std::int32_t* data, std::size_t count) const;
	void SetUniform(const std::string& name, const float* data, std::size_t count) const;
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

private:
	friend class impl::RendererData;

	[[nodiscard]] static std::int32_t GetBoundId();

	[[nodiscard]] std::int32_t GetUniformLocation(const std::string& name) const;

	void CompileProgram(const std::string& vertex_shader, const std::string& fragment_shader);

	// Returns shader id.
	[[nodiscard]] static std::uint32_t CompileShader(std::uint32_t type, const std::string& source);
};

enum class ScreenShader {
	Default,
	Blur,
	EdgeDetection,
	Grayscale,
	InverseColor,
	Sharpen,
};

enum class PresetShader {
	Quad,
	Circle,
	Color
};

namespace impl {

class ShaderManager : public MapManager<Shader> {
public:
	using MapManager::MapManager;

	Shader Get(ScreenShader screen_shader) const;

private:
	friend class Game;
	friend class RendererData;

	Shader Get(PresetShader shader) const;

	void Init();

	// Screen shaders.
	Shader default_;
	Shader blur_;
	Shader grayscale_;
	Shader inverse_color_;
	Shader edge_detection_;
	Shader sharpen_;

	// Preset shaders.
	Shader quad_;
	Shader circle_;
	Shader color_;
};

} // namespace impl

} // namespace ptgn