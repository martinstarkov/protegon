#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "common/assert.h"
#include "debug/log.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "serialization/enum.h"
#include "utility/file.h"

// clang-format off
#define PTGN_SHADER_STRINGIFY_MACRO(x) PTGN_STRINGIFY(x)

// These allow for shaders to differ for Emscripten as it uses OpenGL ES 3.0.
#ifdef __EMSCRIPTEN__
#define PTGN_SHADER_PATH(file) PTGN_SHADER_STRINGIFY_MACRO(PTGN_EXPAND(resources/shader/es/)PTGN_EXPAND(file))
#else
#define PTGN_SHADER_PATH(file) PTGN_SHADER_STRINGIFY_MACRO(PTGN_EXPAND(resources/shader/core/)PTGN_EXPAND(file))
#endif
// clang-format on

namespace ptgn {

// Wrapper for distinguishing between Shader from path construction and Shader
// from source construction.
struct ShaderCode {
	ShaderCode() = default;

	// Explicit construction prevents conflict with Shader path construction.
	explicit ShaderCode(const std::string& source) : source_{ source } {}

	~ShaderCode() = default;

	std::string source_;
};

[[nodiscard]] std::string_view GetShaderName(std::uint32_t shader_type);

using ShaderId = std::uint32_t;

class Shader {
public:
	Shader() = default;
	Shader(
		const ShaderCode& vertex_shader, const ShaderCode& fragment_shader,
		std::string_view shader_name
	);
	Shader(
		const path& vertex_shader_path, const path& fragment_shader_path,
		std::string_view shader_name
	);
	Shader(const Shader&)			 = delete;
	Shader& operator=(const Shader&) = delete;
	Shader(Shader&& other) noexcept;
	Shader& operator=(Shader&& other) noexcept;
	~Shader();

	friend bool operator==(const Shader& a, const Shader& b) {
		return a.id_ == b.id_;
	}

	friend bool operator!=(const Shader& a, const Shader& b) {
		return !(a == b);
	}

	// Sets the uniform value for the specified uniform name. If the uniform does not exist in the
	// shader, nothing happens.
	// Note: Make sure to bind the shader before setting uniforms.
	void SetUniform(const std::string& name, const std::int32_t* data, std::int32_t count) const;
	void SetUniform(const std::string& name, const float* data, std::int32_t count) const;
	void SetUniform(const std::string& name, const Vector2<float>& v) const;
	void SetUniform(const std::string& name, const Vector3<float>& v) const;
	void SetUniform(const std::string& name, const Vector4<float>& v) const;
	void SetUniform(const std::string& name, const Matrix4& matrix) const;
	void SetUniform(const std::string& name, float v0) const;
	void SetUniform(const std::string& name, float v0, float v1) const;
	void SetUniform(const std::string& name, float v0, float v1, float v2) const;
	void SetUniform(const std::string& name, float v0, float v1, float v2, float v3) const;
	void SetUniform(const std::string& name, const Vector2<std::int32_t>& v) const;
	void SetUniform(const std::string& name, const Vector3<std::int32_t>& v) const;
	void SetUniform(const std::string& name, const Vector4<std::int32_t>& v) const;
	void SetUniform(const std::string& name, std::int32_t v0) const;
	void SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1) const;
	void SetUniform(
		const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2
	) const;
	void SetUniform(
		const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2, std::int32_t v3
	) const;

	// Behaves identically to SetUniform(name, std::int32_t).
	void SetUniform(const std::string& name, bool value) const;

	// Bind the shader before setting uniforms.
	void Bind() const;

	// Bind a shader id as the current shader.
	static void Bind(ShaderId id);

	// @return True if the shader is currently bound.
	[[nodiscard]] bool IsBound() const;

	// @return The id of the currently bound shader.
	[[nodiscard]] static ShaderId GetBoundId();

	// @return True if id != 0.
	[[nodiscard]] bool IsValid() const;

	[[nodiscard]] ShaderId GetId() const;

	[[nodiscard]] std::string_view GetName() const;

private:
	void Create();
	void Delete() noexcept;

	[[nodiscard]] std::int32_t GetUniform(const std::string& name) const;

	// Compile program
	void Compile(const std::string& vertex_shader, const std::string& fragment_shader);

	// Compile shader
	[[nodiscard]] static ShaderId Compile(std::uint32_t type, const std::string& source);

	ShaderId id_{ 0 };
	std::string_view shader_name_;

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
	Circle
};

enum class OtherShader {
	Light,
	ToneMapping
};

namespace impl {

class ShaderManager {
public:
	template <auto S>
	[[nodiscard]] const Shader& Get() const {
		using ShaderType = decltype(S);
		if constexpr (std::is_same_v<ShaderType, ShapeShader>) {
			if constexpr (S == ShapeShader::Quad) {
				return quad_;
			} else if constexpr (S == ShapeShader::Circle) {
				return circle_;
			} else {
				PTGN_ERROR("Cannot retrieve unrecognized circle shader");
			}
		} else if constexpr (std::is_same_v<ShaderType, OtherShader>) {
			if constexpr (S == OtherShader::Light) {
				return light_;
			} else if constexpr (S == OtherShader::ToneMapping) {
				return tone_mapping_;
			} else {
				PTGN_ERROR("Cannot retrieve unrecognized other shader");
			}
		} else if constexpr (std::is_same_v<ShaderType, ScreenShader>) {
			if constexpr (S == ScreenShader::Default) {
				return default_;
			} else if constexpr (S == ScreenShader::Blur) {
				return blur_;
			} else if constexpr (S == ScreenShader::GaussianBlur) {
				return gaussian_blur_;
			} else if constexpr (S == ScreenShader::EdgeDetection) {
				return edge_detection_;
			} else if constexpr (S == ScreenShader::InverseColor) {
				return inverse_color_;
			} else if constexpr (S == ScreenShader::Grayscale) {
				return grayscale_;
			} else if constexpr (S == ScreenShader::Sharpen) {
				return sharpen_;
			} else {
				PTGN_ERROR("Cannot retrieve unrecognized screen shader");
			}
		} else {
			PTGN_ERROR("Cannot retrieve unrecognized shader type");
		}
	}

private:
	friend class Game;

	void Init();

	// Note: Defined in header to ensure that changing a shader will recompile the necessary files.

	void InitShapeShaders() {
		// Quad is initialized in cpp because it depends on texture slots.

		circle_ = { ShaderCode{
#include PTGN_SHADER_PATH(quad.vert)
					},
					ShaderCode{
#include PTGN_SHADER_PATH(circle.frag)
					},
					"Circle" };
	}

	void InitScreenShaders() {
		default_ = { ShaderCode{
#include PTGN_SHADER_PATH(screen_default.vert)
					 },
					 ShaderCode{
#include PTGN_SHADER_PATH(screen_default.frag)
					 },
					 "Default" };

		blur_ = { ShaderCode{
#include PTGN_SHADER_PATH(screen_default.vert)
				  },
				  ShaderCode{
#include PTGN_SHADER_PATH(screen_blur.frag)
				  },
				  "Blur" };

		gaussian_blur_ = { ShaderCode{
#include PTGN_SHADER_PATH(screen_default.vert)
						   },
						   ShaderCode{
#include PTGN_SHADER_PATH(screen_gaussian_blur.frag)
						   },
						   "Gaussian Blur" };

		edge_detection_ = { ShaderCode{
#include PTGN_SHADER_PATH(screen_default.vert)
							},
							ShaderCode{
#include PTGN_SHADER_PATH(screen_edge_detection.frag)
							},
							"Edge Detection" };

		grayscale_ = { ShaderCode{
#include PTGN_SHADER_PATH(screen_default.vert)
					   },
					   ShaderCode{
#include PTGN_SHADER_PATH(screen_grayscale.frag)
					   },
					   "Grayscale" };

		inverse_color_ = { ShaderCode{
#include PTGN_SHADER_PATH(screen_default.vert)
						   },
						   ShaderCode{
#include PTGN_SHADER_PATH(screen_inverse_color.frag)
						   },
						   "Inverse Color" };

		sharpen_ = { ShaderCode{
#include PTGN_SHADER_PATH(screen_default.vert)
					 },
					 ShaderCode{
#include PTGN_SHADER_PATH(screen_sharpen.frag)
					 },
					 "Sharpen" };
	}

	void InitOtherShaders() {
		light_ = Shader(
			ShaderCode{
#include PTGN_SHADER_PATH(screen_default.vert)
			},
			ShaderCode{
#include PTGN_SHADER_PATH(lighting.frag)
			},
			"Light"
		);

		tone_mapping_ = Shader(
			ShaderCode{
#include PTGN_SHADER_PATH(screen_default.vert)
			},
			ShaderCode{
#include PTGN_SHADER_PATH(tone_mapping.frag)
			},
			"Tone Mapping"
		);
	}

	// Screen shaders.
	Shader default_;
	Shader blur_;
	Shader gaussian_blur_;
	Shader grayscale_;
	Shader inverse_color_;
	Shader edge_detection_;
	Shader sharpen_;

	// Color shaders.
	Shader quad_;
	Shader circle_;

	// Other shaders.
	Shader light_;
	Shader tone_mapping_;
};

} // namespace impl

PTGN_SERIALIZER_REGISTER_ENUM(
	ScreenShader, { { ScreenShader::Default, "default" },
					{ ScreenShader::Blur, "blur" },
					{ ScreenShader::GaussianBlur, "gaussian_blur" },
					{ ScreenShader::EdgeDetection, "edge_detection" },
					{ ScreenShader::Grayscale, "grayscale" },
					{ ScreenShader::InverseColor, "inverse_color" },
					{ ScreenShader::Sharpen, "sharpen" } }
);

PTGN_SERIALIZER_REGISTER_ENUM(
	ShapeShader, { { ShapeShader::Quad, "quad" }, { ShapeShader::Circle, "circle" } }
);

PTGN_SERIALIZER_REGISTER_ENUM(
	OtherShader, { { OtherShader::Light, "light" }, { OtherShader::ToneMapping, "tone_mapping" } }
);

} // namespace ptgn