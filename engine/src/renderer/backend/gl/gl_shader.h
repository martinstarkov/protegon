#pragma once

#include <cmrc/cmrc.hpp>
#include <cstdint>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/file.h"
#include "core/util/id_map.h"
#include "serialization/json/fwd.h"

CMRC_DECLARE(shader);

namespace ptgn {

struct Matrix4;

namespace impl::gl {

using Program = std::uint32_t;
using Shader  = std::uint32_t;

struct ShaderCode {
	std::string content;
};

struct ShaderOptions {
	bool auto_layout{ false };
};

enum class ShaderType : std::uint32_t {
	Vertex		   = 0x8B31, // GL_VERTEX_SHADER
	Fragment	   = 0x8B30, // GL_FRAGMENT_SHADER
	Geometry	   = 0x8DD9, // GL_GEOMETRY_SHADER
	TessControl	   = 0x8E88, // GL_TESS_CONTROL_SHADER
	TessEvaluation = 0x8E87, // GL_TESS_EVALUATION_SHADER
	Compute		   = 0x91B9	 // GL_COMPUTE_SHADER
};

inline std::ostream& operator<<(std::ostream& os, ShaderType type) {
	switch (type) {
		using enum ptgn::impl::gl::ShaderType;

		case Vertex:		 return os << "Vertex";
		case Fragment:		 return os << "Fragment";
		case Geometry:		 return os << "Geometry";
		case TessControl:	 return os << "TessControl";
		case TessEvaluation: return os << "TessEvaluation";
		case Compute:		 return os << "Compute";
		default:			 return os << "Unknown ShaderType";
	}
}

using ShaderName = std::string;

struct ShaderSpec {
	ShaderType type{ ShaderType::Fragment };
	ShaderCode code;
	ShaderName name;
	ShaderOptions options;
};

struct ProgramCache {
	std::string program_name;
	bool batchable{ false };

	// cache needs to be mutable even in const functions.
	mutable std::unordered_map<std::size_t, std::int32_t> uniform_locations;
};

class Shaders {
public:
	Program CreateProgram(Shader vertex, Shader fragment, const std::string& program_name);

	Shader CreateProgram(
		const std::variant<ShaderCode, ShaderName>& vertex,
		const std::variant<ShaderCode, ShaderName>& fragment, const std::string& program_name
	);

	Program CreateProgram(
		const path& vertex_path, const path& fragment_path, const std::string& program_name
	);

	Program CreateProgram(
		const std::variant<ShaderCode, path>& source, const std::string& program_name
	);

	void SetUniform(Program id, const char* uniform_name, V2_float v);
	void SetUniform(Program id, const char* uniform_name, V3_float v);
	void SetUniform(Program id, const char* uniform_name, V4_float v);
	void SetUniform(Program id, const char* uniform_name, const Matrix4& matrix);
	void SetUniform(
		Program id, const char* uniform_name, const std::int32_t* data, std::int32_t count
	);
	void SetUniform(Program id, const char* uniform_name, const float* data, std::int32_t count);
	void SetUniform(Program id, const char* uniform_name, const Vector2<std::int32_t>& v);
	void SetUniform(Program id, const char* uniform_name, const Vector3<std::int32_t>& v);
	void SetUniform(Program id, const char* uniform_name, const Vector4<std::int32_t>& v);

	void SetUniform(Program id, const char* uniform_name, float v0);
	void SetUniform(Program id, const char* uniform_name, float v0, float v1);
	void SetUniform(Program id, const char* uniform_name, float v0, float v1, float v2);
	void SetUniform(Program id, const char* uniform_name, float v0, float v1, float v2, float v3);
	void SetUniform(Program id, const char* uniform_name, std::int32_t v0);
	void SetUniform(Program id, const char* uniform_name, std::int32_t v0, std::int32_t v1);
	void SetUniform(
		Program id, const char* uniform_name, std::int32_t v0, std::int32_t v1, std::int32_t v2
	);
	void SetUniform(
		Program id, const char* uniform_name, std::int32_t v0, std::int32_t v1, std::int32_t v2,
		std::int32_t v3
	);
	// Behaves identically to SetUniform(name, std::int32_t).
	void SetUniform(Program id, const char* uniform_name, bool value);

	[[nodiscard]] Program GetProgram(std::string_view program_name) const;

	void DestroyProgram(Program id);

private:
	friend class GLContext;

	Shaders(GLContext& gl);
	~Shaders() noexcept;
	Shaders(const Shaders&)				   = delete;
	Shaders(Shaders&&) noexcept			   = delete;
	Shaders& operator=(const Shaders&)	   = delete;
	Shaders& operator=(Shaders&&) noexcept = delete;

	std::vector<ShaderSpec> ParseShaderSourceFile(
		const std::string& source, const std::string& name
	) const;

	Shader CompileShaderSource(const std::string& source, ShaderType type, const std::string& name);

	Shader CompileShaderPath(const path& shader_path, ShaderType type, const std::string& name);

	void CompileShaders(const std::vector<ShaderSpec>& sources);

	void Populate(std::size_t max_texture_slots);

	void PopulateShadersFromCache(const json& manifest);

	void PopulateShaderCache(const cmrc::embedded_filesystem& filesystem);

	bool ShaderExists(std::string_view shader_name, ShaderType type) const;

	Shader GetShaderId(std::string_view shader_name, ShaderType type) const;

	std::pair<Shader, bool> GetShaderIdWithDeleteFlag(
		const std::variant<ShaderCode, std::string>& variant, ShaderType type,
		const std::string& shader_name
	);

	[[nodiscard]] static Shader CompileShader(ShaderType type, const std::string& source);

	void CompileProgram(
		Program id, const std::string& vertex_source, const std::string& fragment_source
	) const;

	void LinkProgram(Program id, Shader vertex, Shader fragment);

	[[nodiscard]] std::int32_t GetUniform(Program id, const char* name);

	[[nodiscard]] Program CreateProgram(const std::string& program_name);

	GLContext& gl_;

	std::size_t max_texture_slots_{ 0 };

	std::unordered_map<std::size_t, Program> programs_;

	std::unordered_map<std::size_t, Shader> vertex_shaders_;
	std::unordered_map<std::size_t, Shader> fragment_shaders_;

	IdMap<ProgramCache> cache_;
};

} // namespace impl::gl

} // namespace ptgn