#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/file.h"
#include "core/util/id_map.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"
#include "runtime/asset/engine_shader_library.h"
#include "serialization/json/fwd.h"

namespace ptgn::impl::gl {

class GLContext;

enum class ShaderType : std::uint32_t {
	Vertex = 0x8B31,
	Fragment = 0x8B30,
	Geometry = 0x8DD9,
	TessControl = 0x8E88,
	TessEvaluation = 0x8E87,
	Compute = 0x91B9
};

struct ShaderSpec {
	ShaderType type{ ShaderType::Fragment };
	ShaderCode code{};
	ShaderName name{};
};

struct ShaderInfo {
	ShaderId id{};
	bool delete_after{ false };
};

struct ProgramInfo {
	ShaderInfo vertex{};
	ShaderInfo fragment{};
};

struct ProgramCache {
	std::string program_name{};
	mutable std::unordered_map<std::size_t, std::int32_t> uniform_locations{};
};

class Shaders {
public:
	ShaderId CreateProgram(ShaderId vertex, ShaderId fragment, std::string_view program_name);

	ShaderId CreateProgram(
		const std::variant<ShaderCode, ShaderPath, ShaderPair>& source,
		std::string_view program_name
	);

	void SetUniform(ShaderId id, const char* uniform_name, const Matrix4& v);
	void SetUniform(ShaderId id, const char* uniform_name, float v);
	void SetUniform(ShaderId id, const char* uniform_name, V2_float v);
	void SetUniform(ShaderId id, const char* uniform_name, V3_float v);
	void SetUniform(ShaderId id, const char* uniform_name, V4_float v);
	void SetUniform(ShaderId id, const char* uniform_name, std::span<const float> v);
	void SetUniform(ShaderId id, const char* uniform_name, int v);
	void SetUniform(ShaderId id, const char* uniform_name, V2_int v);
	void SetUniform(ShaderId id, const char* uniform_name, V3_int v);
	void SetUniform(ShaderId id, const char* uniform_name, V4_int v);
	void SetUniform(ShaderId id, const char* uniform_name, std::span<const int> v);
	void SetUniform(ShaderId id, const char* uniform_name, bool v);

	ShaderId GetProgram(std::string_view program_name) const;

	[[nodiscard]] std::span<const std::string> GetVertexShaderNames() const;
	[[nodiscard]] std::span<const std::string> GetFragmentShaderNames() const;

	void DestroyProgram(ShaderId id);

private:
	friend class GLContext;

	explicit Shaders(GLContext& gl, std::size_t max_texture_slots);
	~Shaders() noexcept;
	Shaders(const Shaders&) = delete;
	Shaders(Shaders&&) noexcept = delete;
	Shaders& operator=(const Shaders&) = delete;
	Shaders& operator=(Shaders&&) noexcept = delete;

	std::vector<ShaderSpec> ParseShaderSourceFile(
		const std::string& source,
		std::string_view name
	) const;

	ShaderId CompileShaderSource(
		const std::string& source,
		ShaderType type,
		std::string_view name
	) const;

	ShaderId CompileShaderPath(
		const path& shader_path,
		ShaderType type,
		std::string_view name
	) const;

	void CompileShaders(const std::vector<ShaderSpec>& sources);
	void PopulateShadersFromCache(const json& manifest);
	void PopulateShaderCache(std::span<const ::ptgn::impl::EngineShaderFile> files);

	bool ShaderExists(std::string_view shader_name, ShaderType type) const;
	ShaderId GetShaderId(std::string_view shader_name, ShaderType type) const;

	ProgramInfo GetProgramInfo(
		const std::variant<ShaderCode, ShaderPath>& code_or_path,
		std::string_view program_name
	) const;

	ProgramInfo GetProgramInfo(
		const ShaderPair& shader_pair,
		std::string_view program_name
	) const;

	ProgramInfo GetProgramInfo(
		const std::variant<ShaderCode, ShaderPath, ShaderPair>& variant,
		std::string_view program_name
	) const;

	ShaderInfo GetShaderInfo(
		const std::variant<ShaderCode, ShaderPathOrName>& variant,
		ShaderType type,
		std::string_view shader_name
	) const;

	ShaderInfo GetShaderInfo(
		const ShaderCode& code,
		ShaderType type,
		std::string_view shader_name
	) const;

	ShaderInfo GetShaderInfo(
		const ShaderPathOrName& path_or_name,
		ShaderType type,
		std::string_view shader_name
	) const;

	[[nodiscard]] ShaderId CompileShader(ShaderType type, const std::string& source) const;

	void CompileProgram(
		ShaderId id,
		const std::string& vertex_source,
		const std::string& fragment_source
	) const;

	void LinkProgram(ShaderId id, ShaderId vertex, ShaderId fragment);
	std::int32_t GetUniform(ShaderId id, const char* uniform_name);
	[[nodiscard]] ShaderId CreateProgram(std::string_view program_name);

	GLContext& gl_;
	std::size_t max_texture_slots_{ 0 };
	std::unordered_map<std::size_t, ShaderId> programs_;
	std::unordered_map<std::size_t, ShaderId> vertex_shaders_;
	std::unordered_map<std::size_t, ShaderId> fragment_shaders_;
	std::vector<std::string> vertex_shader_names_;
	std::vector<std::string> fragment_shader_names_;
	IdMap<ProgramCache> cache_;
};

} // namespace ptgn::impl::gl
