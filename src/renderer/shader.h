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

// clang-format off
#define PTGN_SHADER_STRINGIFY_MACRO(x) PTGN_STRINGIFY_MACRO(x)

// These allow for shaders to differ for Emscripten as it uses OpenGL ES 3.0.
#ifdef __EMSCRIPTEN__
#define PTGN_SHADER_PATH(file) PTGN_SHADER_STRINGIFY_MACRO(PTGN_EXPAND_MACRO(resources/shader/es/)PTGN_EXPAND_MACRO(file))
#else
#define PTGN_SHADER_PATH(file) PTGN_SHADER_STRINGIFY_MACRO(PTGN_EXPAND_MACRO(resources/shader/core/)PTGN_EXPAND_MACRO(file))
#endif
// clang-format on

namespace ptgn::impl {

// Wrapper for distinguishing between Shader from path construction and Shader
// from source construction.
struct ShaderSource {
	ShaderSource() = default;

	// Explicit construction prevents conflict with Shader path construction.
	explicit ShaderSource(const std::string& source) : source_{ source } {}

	~ShaderSource() = default;

	std::string source_;
};

[[nodiscard]] std::string_view GetShaderName(std::uint32_t shader_type);

struct Shader {
	Shader() = default;
	Shader(const ShaderSource& vertex_shader, const ShaderSource& fragment_shader);
	Shader(const path& vertex_shader_path, const path& fragment_shader_path);
	Shader(const Shader&)			 = delete;
	Shader& operator=(const Shader&) = delete;
	Shader(Shader&& other) noexcept;
	Shader& operator=(Shader&& other) noexcept;
	~Shader();

	// Sets the uniform value for the specified uniform name. If the uniform does not exist in the
	// shader, nothing happens.
	// Note: Make sure to bind the shader before setting uniforms.
	void SetUniform(const std::string& name, const std::int32_t* data, std::int32_t count) const;
	void SetUniform(const std::string& name, const float* data, std::int32_t count) const;
	void SetUniform(const std::string& name, const Vector2<float>& v) const;
	void SetUniform(const std::string& name, const Vector3<float>& v) const;
	void SetUniform(const std::string& name, const Vector4<float>& v) const;
	void SetUniform(const std::string& name, const Matrix4& m) const;
	void SetUniform(const std::string& name, float v0) const;
	void SetUniform(const std::string& name, float v0, float v1) const;
	void SetUniform(const std::string& name, float v0, float v1, float v2) const;
	void SetUniform(const std::string& name, float v0, float v1, float v2, float v3) const;
	void SetUniform(const std::string& name, const Vector2<std::int32_t>& v) const;
	void SetUniform(const std::string& name, const Vector3<std::int32_t>& v) const;
	void SetUniform(const std::string& name, const Vector4<std::int32_t>& v) const;
	void SetUniform(const std::string& name, std::int32_t v0) const;
	void SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1) const;
	void SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2)
		const;
	void SetUniform(
		const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2, std::int32_t v3
	) const;

	// Behaves identically to SetUniform(name, std::int32_t).
	void SetUniform(const std::string& name, bool value) const;

	// Bind the shader before setting uniforms.
	void Bind() const;

	// Bind a shader id as the current shader.
	static void Bind(std::uint32_t id);

	// @return True if the shader is currently bound.
	[[nodiscard]] bool IsBound() const;

	// @return The id of the currently bound shader.
	[[nodiscard]] static std::uint32_t GetBoundId();

	// @return True if id != 0.
	[[nodiscard]] bool IsValid() const;
private:
	void CreateProgram();
	void DeleteProgram() noexcept;

	[[nodiscard]] std::int32_t GetUniformLocation(const std::string& name) const;

	void CompileProgram(const std::string& vertex_shader, const std::string& fragment_shader);

	[[nodiscard]] static std::uint32_t CompileShader(std::uint32_t type, const std::string& source);

	std::uint32_t id_{ 0 };

	// Location cache should not prevent const calls.
	mutable std::unordered_map<std::string, std::int32_t> location_cache_;
};

// Note: If applicable, TextureInfo tint is applied after shader effect.
enum class ScreenShader {
	Default,
	Blur,
	GaussianBlur,
	EdgeDetection,
	Grayscale,
	InverseColor,
	Sharpen,
};

enum class ShapeShader {
	Quad,
	Circle,
	Color
};

class ShaderManager : public MapManager<Shader> {
public:
	ShaderManager()									   = default;
	~ShaderManager() override						   = default;
	ShaderManager(ShaderManager&&) noexcept			   = default;
	ShaderManager& operator=(ShaderManager&&) noexcept = default;
	ShaderManager(const ShaderManager&)				   = delete;
	ShaderManager& operator=(const ShaderManager&)	   = delete;

	Shader Get(ScreenShader screen_shader) const;

	Shader Get(ShapeShader shader) const;

private:
	friend class Game;
	friend class RenderData;

	void Init();

	// Note: Defined in header to ensure that changing a shader will recompile the necessary files.
	void InitScreenShaders() {
		// TODO: Add these shaders for emscripten OpenGL ES3.

		default_ = { ShaderSource{
#include PTGN_SHADER_PATH(screen_default.vert)
					 },
					 ShaderSource{
#include PTGN_SHADER_PATH(screen_default.frag)
					 } };

		blur_ = { ShaderSource{
#include PTGN_SHADER_PATH(screen_default.vert)
				  },
				  ShaderSource{
#include PTGN_SHADER_PATH(screen_blur.frag)
				  } };

		gaussian_blur_ = { ShaderSource{
#include PTGN_SHADER_PATH(screen_default.vert)
						   },
						   ShaderSource{
#include PTGN_SHADER_PATH(screen_gaussian_blur.frag)
						   } };

		edge_detection_ = { ShaderSource{
#include PTGN_SHADER_PATH(screen_default.vert)
							},
							ShaderSource{
#include PTGN_SHADER_PATH(screen_edge_detection.frag)
							} };

		grayscale_ = { ShaderSource{
#include PTGN_SHADER_PATH(screen_default.vert)
					   },
					   ShaderSource{
#include PTGN_SHADER_PATH(screen_grayscale.frag)
					   } };

		inverse_color_ = { ShaderSource{
#include PTGN_SHADER_PATH(screen_default.vert)
						   },
						   ShaderSource{
#include PTGN_SHADER_PATH(screen_inverse_color.frag)
						   } };

		sharpen_ = { ShaderSource{
#include PTGN_SHADER_PATH(screen_default.vert)
					 },
					 ShaderSource{
#include PTGN_SHADER_PATH(screen_sharpen.frag)
					 } };
	}

	// TODO: Make these hidden and remove duplicates.
	Shader default_screen_shader;
	Shader quad_shader;
	Shader circle_shader;
	Shader light_shader;

	// Screen shaders.
	Shader default_;
	Shader blur_;
	Shader gaussian_blur_;
	Shader grayscale_;
	Shader inverse_color_;
	Shader edge_detection_;
	Shader sharpen_;

	// Preset shaders.
	Shader quad_;
	Shader circle_;
	Shader color_;
};

} // namespace ptgn::impl