#pragma once

#include <cmrc/cmrc.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "debug/log.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "serialization/enum.h"
#include "utility/file.h"

CMRC_DECLARE(shader);

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

enum class ShaderType : std::uint32_t {
	Vertex	 = 0x8B31, // GL_VERTEX_SHADER
	Fragment = 0x8B30, // GL_FRAGMENT_SHADER
					   /*
						   Compute		   = 0x91B9, // GL_COMPUTE_SHADER
						   Geometry	   = 0x8DD9, // GL_GEOMETRY_SHADER
						   TessEvaluation = 0x8E87, // GL_TESS_EVALUATION_SHADER
						   TessControl	   = 0x8E88	 // GL_TESS_CONTROL_SHADER
					   */
};

inline std::ostream& operator<<(std::ostream& os, ShaderType type) {
	switch (type) {
		case ShaderType::Vertex:   os << "Vertex"; break;
		case ShaderType::Fragment: os << "Fragment"; break;
		/*
		case ShaderType::Compute:		 os << "Compute"; break;
		case ShaderType::Geometry:		 os << "Geometry"; break;
		case ShaderType::TessEvaluation: os << "TessEvaluation"; break;
		case ShaderType::TessControl:	 os << "TessControl"; break;
		*/
		default:				   PTGN_ERROR("Unrecognized shader type")
	}
	return os;
}

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
	Shader(
		const ShaderCode& vertex_shader, const path& fragment_shader_path,
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
	[[nodiscard]] static ShaderId Compile(ShaderType type, const std::string& source);

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
		std::shared_ptr<Shader> test{ std::make_shared<Shader>() };
		return *test;
	}

private:
	friend class Game;

	void Init();
};

} // namespace impl

} // namespace ptgn