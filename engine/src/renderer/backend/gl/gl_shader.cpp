#include "renderer/backend/gl/gl_shader.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <list>
#include <nlohmann/json.hpp>
#include <ostream>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/file.h"
#include "core/util/hash.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"
#include "renderer/pipeline/shader_preprocessor.h"
#include "runtime/asset/engine_shader_library.h"
#include "serialization/json/fwd.h"

namespace ptgn::impl::gl {

namespace {

void DeleteShaderId(ShaderId id, [[maybe_unused]] ShaderType type) {
	GLCall(glDeleteShader(id));
}

void DeleteProgramId(ShaderId id) {
	GLCall(glDeleteProgram(id));
}

void LinkProgramId(ShaderId id) {
	GLCall(glLinkProgram(id));
}

ShaderType ToShaderType(ShaderStageMask stage) {
	if (stage == ShaderStageMask::Vertex) {
		return ShaderType::Vertex;
	}
	if (stage == ShaderStageMask::Fragment) {
		return ShaderType::Fragment;
	}
	PTGN_ERROR("Unknown shader stage");
}

std::string_view GetShaderName(ShaderType type) {
	switch (type) {
		using enum ShaderType;
		case Vertex:         return "vertex";
		case Fragment:       return "fragment";
		case Geometry:       return "geometry";
		case TessControl:    return "tess_control";
		case TessEvaluation: return "tess_evaluation";
		case Compute:        return "compute";
		default:             PTGN_ERROR("Unknown shader type: ", std::to_underlying(type));
	}
}

} // namespace

ShaderId Shaders::CompileShader(ShaderType type, const std::string& source) const {
	ShaderId id{ GLCallReturn(glCreateShader(std::to_underlying(type))) };

	auto src{ source.c_str() };

	GLCall(glShaderSource(id, 1, &src, nullptr));
	GLCall(glCompileShader(id));

	// Check for shader compilation errors.
	std::int32_t result{ GL_FALSE };
	GLCall(glGetShaderiv(id, GL_COMPILE_STATUS, &result));

	if (result == GL_FALSE) {
		std::int32_t length{ 0 };
		GLCall(glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length));
		std::string log;
		log.resize(static_cast<std::size_t>(length));
		GLCall(glGetShaderInfoLog(id, length, &length, &log[0]));

		DeleteShaderId(id, type);

		PTGN_ERROR("Failed to compile ", GetShaderName(type), " shader: \n", source, "\n", log);
	}

	return id;
}

void Shaders::CompileShaders(const std::vector<ShaderSpec>& sources) {
	for (const auto& sts : sources) {
		auto hash{ Hash(sts.name) };
		auto shader_id{ CompileShader(sts.type, sts.code.content) };

		switch (sts.type) {
			case ShaderType::Fragment:
				PTGN_ASSERT(
					!fragment_shaders_.contains(hash),
					"Cannot add shader to cache twice: ", sts.name
				);
				fragment_shaders_.emplace(hash, shader_id);
				fragment_shader_names_.emplace_back(sts.name);
				break;
			case ShaderType::Vertex:
				PTGN_ASSERT(
					!vertex_shaders_.contains(hash), "Cannot add shader to cache twice: ", sts.name
				);
				vertex_shaders_.emplace(hash, shader_id);
				vertex_shader_names_.emplace_back(sts.name);
				break;
			default: PTGN_ERROR("Unknown shader type");
		}
	}
}

void Shaders::PopulateShaderCache(std::span<const ::ptgn::impl::EngineShaderFile> files) {
	std::vector<ShaderSpec> sources;

	for (const auto& shader_file : files) {
		std::string name_without_ext{ shader_file.filename.stem().string() };
		auto parsed{ ParseShaderSourceFile(shader_file.source, name_without_ext) };
		std::ranges::move(parsed, std::back_inserter(sources));
	}

	CompileShaders(sources);
}

void Shaders::PopulateShadersFromCache(const json& manifest) {
	PTGN_ASSERT(manifest.is_object(), "Shader manifest must be a json object");
	for (const auto& [shader_name, shader_object] : manifest.items()) {
		std::string vertex_name;
		std::string fragment_name;

		if (shader_object.contains("vertex") && shader_object.contains("fragment")) {
			vertex_name	  = shader_object.at("vertex").get<std::string>();
			fragment_name = shader_object.at("fragment").get<std::string>();
		} else if (shader_object.contains("source")) {
			auto name	  = shader_object.at("source").get<std::string>();
			vertex_name	  = name;
			fragment_name = name;
		} else {
			PTGN_ERROR(
				"Manifest shader ", shader_name,
				" must specify either a 'vertex' and 'fragment' property for individual "
				"specification, or a combined 'source' "
				"property for same-name vertex/fragment shaders, instead it is: ",
				shader_object.dump(4)
			);
		}

		auto vert_hash{ Hash(vertex_name) };
		auto frag_hash{ Hash(fragment_name) };

		PTGN_ASSERT(
			vertex_shaders_.contains(vert_hash), "Vertex shader: ", vertex_name, " for ",
			shader_name, " not found in shader directory"
		);

		PTGN_ASSERT(
			fragment_shaders_.contains(frag_hash), "Fragment shader: ", fragment_name, " for ",
			shader_name, " not found in shader directory"
		);

		auto vert_id{ vertex_shaders_.find(vert_hash)->second };
		auto frag_id{ fragment_shaders_.find(frag_hash)->second };

		auto hash{ Hash(shader_name) };

		PTGN_ASSERT(!programs_.contains(hash), "ShaderId names in the manifest must be unique");

		auto program{ CreateProgram(shader_name) };

		LinkProgram(program, vert_id, frag_id);

		programs_.emplace(hash, program);
	}
}

std::vector<ShaderSpec> Shaders::ParseShaderSourceFile(
	const std::string& source, std::string_view name
) const {
	auto prepared{ ::ptgn::impl::PrepareShaderSource(source, max_texture_slots_) };
	if (!prepared.has_value()) {
		PTGN_ERROR("Failed to preprocess shader '", name, "': ", prepared.error());
	}

	std::vector<ShaderSpec> result;
	result.reserve(prepared->size());

	for (auto& stage : prepared.value()) {
		ShaderCode code;
		code.content = std::move(stage.source);

		result.push_back(ShaderSpec{
			.type = ToShaderType(stage.stage),
			.code = std::move(code),
			.name = std::string{ name },
		});
	}

	return result;
}

ShaderId Shaders::CompileShaderSource(
	const std::string& source, ShaderType type, std::string_view name
) const {
	auto srcs{ ParseShaderSourceFile(source, name) };
	PTGN_ASSERT(srcs.size() == 1, "Wrong constructor for a multi-source shader file");
	const auto& front{ srcs.front() };
	PTGN_ASSERT(front.type == type, "ShaderId type mismatch");
	return CompileShader(type, front.code.content);
}

ShaderId Shaders::CompileShaderPath(
	const path& shader_path, ShaderType type, std::string_view name
) const {
	PTGN_ASSERT(
		FileExists(shader_path),
		"Cannot create shader from nonexistent shader path: ", shader_path.string()
	);
	auto source{ FileToString(shader_path) };
	return CompileShaderSource(source, type, name);
}

void Shaders::LinkProgram(ShaderId id, ShaderId vertex, ShaderId fragment) {
	cache_.Get(id).uniform_locations.clear();

	PTGN_ASSERT(vertex);
	PTGN_ASSERT(fragment);

	GLCall(glAttachShader(id, vertex));
	GLCall(glAttachShader(id, fragment));

	LinkProgramId(id);

	// Check for shader link errors.
	std::int32_t linked{ GL_FALSE };
	GLCall(glGetProgramiv(id, GL_LINK_STATUS, &linked));

	if (linked == GL_FALSE) {
		std::int32_t length{ 0 };
		GLCall(glGetProgramiv(id, GL_INFO_LOG_LENGTH, &length));
		std::string log;
		log.resize(static_cast<std::size_t>(length));
		GLCall(glGetProgramInfoLog(id, length, &length, &log[0]));

		DeleteProgramId(id);

		DeleteShaderId(vertex, ShaderType::Vertex);
		DeleteShaderId(fragment, ShaderType::Fragment);

		PTGN_ERROR(
			"Failed to link shaders to program:\nVertex : ", vertex, "\nFragment : ", fragment,
			"\n ", log
		);
	}

	GLCall(glValidateProgram(id));
}

void Shaders::CompileProgram(
	ShaderId id, const std::string& vertex_source, const std::string& fragment_source
) const {
	PTGN_ASSERT(
		cache_.Get(id).uniform_locations.empty(),
		"Shader program uniform cache must be clear when compiling the shader program"
	);

	auto vertex{ CompileShader(ShaderType::Vertex, vertex_source) };
	auto fragment{ CompileShader(ShaderType::Fragment, fragment_source) };

	if (vertex && fragment) {
		GLCall(glAttachShader(id, vertex));
		GLCall(glAttachShader(id, fragment));
		LinkProgramId(id);

		// Check for shader link errors.
		std::int32_t linked{ GL_FALSE };
		GLCall(glGetProgramiv(id, GL_LINK_STATUS, &linked));

		if (linked == GL_FALSE) {
			std::int32_t length{ 0 };
			GLCall(glGetProgramiv(id, GL_INFO_LOG_LENGTH, &length));
			std::string log;
			log.resize(static_cast<std::size_t>(length));
			GLCall(glGetProgramInfoLog(id, length, &length, &log[0]));

			DeleteProgramId(id);

			DeleteShaderId(vertex, ShaderType::Vertex);
			DeleteShaderId(fragment, ShaderType::Fragment);

			PTGN_ERROR(
				"Failed to link shaders to program: \n", vertex_source, "\n", fragment_source, "\n",
				log
			);
		}

		GLCall(glValidateProgram(id));
	}

	if (vertex) {
		DeleteShaderId(vertex, ShaderType::Vertex);
	}

	if (fragment) {
		DeleteShaderId(fragment, ShaderType::Fragment);
	}
}

Shaders::Shaders(GLContext& gl, std::size_t max_texture_slots) :
	gl_{ gl }, max_texture_slots_{ max_texture_slots } {
	PTGN_ASSERT(max_texture_slots > 0, "Platform must support at least one texture slot");

	PopulateShaderCache(::ptgn::impl::GetEngineShaderFiles());
	PopulateShadersFromCache(::ptgn::impl::GetEngineShaderManifest());
}

Shaders::~Shaders() noexcept {
	const auto delete_shaders = [](const auto& container, auto type) {
		for (const auto& [hash, id] : container) {
			if (id) {
				DeleteShaderId(id, type);
			}
		}
	};

	// Delete cached vertex and fragment shaders.
	delete_shaders(vertex_shaders_, ShaderType::Vertex);
	delete_shaders(fragment_shaders_, ShaderType::Fragment);

	// Must be done before deleting context.
	programs_.clear();
}

ShaderId Shaders::CreateProgram(ShaderId vertex, ShaderId fragment, std::string_view program_name) {
	auto shader{ CreateProgram(program_name) };

	LinkProgram(shader, vertex, fragment);

	return shader;
}

bool Shaders::ShaderExists(std::string_view shader_name, ShaderType type) const {
	auto hash{ Hash(shader_name) };
	switch (type) {
		case ShaderType::Fragment: return fragment_shaders_.contains(hash);
		case ShaderType::Vertex:   return vertex_shaders_.contains(hash);
		default:				   PTGN_ERROR("Unknown shader type");
	}
}

ShaderId Shaders::GetShaderId(std::string_view shader_name, ShaderType type) const {
	auto hash{ Hash(shader_name) };
	switch (type) {
		case ShaderType::Fragment: {
			PTGN_ASSERT(
				fragment_shaders_.contains(hash),
				"Could not find fragment shader with name: ", shader_name
			);
			return fragment_shaders_.find(hash)->second;
		}
		case ShaderType::Vertex: {
			PTGN_ASSERT(
				vertex_shaders_.contains(hash),
				"Could not find vertex shader with name: ", shader_name
			);
			return vertex_shaders_.find(hash)->second;
		}
		default: PTGN_ERROR("Unknown shader type");
	}
}

ShaderInfo Shaders::GetShaderInfo(
	const ShaderCode& code, ShaderType type, std::string_view shader_name
) const {
	return { CompileShaderSource(code.content, type, shader_name), true };
}

ShaderInfo Shaders::GetShaderInfo(
	const ShaderPathOrName& path_or_name, ShaderType type, std::string_view shader_name // NOSONAR
) const {
	if (IsFilePath(path_or_name)) {
		const path file_path{
			GetAbsolutePath(path{ path_or_name })
		};

		PTGN_ASSERT(
			FileExists(file_path),
			"Cannot create shader from non-existent file path: ", file_path.string()
		);

		return { CompileShaderPath(file_path, type, shader_name), true };
	}

	PTGN_ASSERT(
		!IsDirectoryPath(path_or_name), "Cannot create shader from directory path: ", path_or_name
	);

	if (ShaderExists(path_or_name, type)) {
		return { GetShaderId(path_or_name, type), false };
	}

	PTGN_ERROR(
		path_or_name, " is not a valid shader path or loaded ", GetShaderName(type), " shader name"
	);
}

ShaderInfo Shaders::GetShaderInfo(
	const std::variant<ShaderCode, ShaderPathOrName>& variant, ShaderType type,
	std::string_view shader_name
) const {
	return std::visit(
		[this, type, shader_name](const auto& arg) -> ShaderInfo {
			return GetShaderInfo(arg, type, shader_name);
		},
		variant
	);
}

ProgramInfo Shaders::GetProgramInfo(
	const ShaderPair& shader_pair, std::string_view program_name
) const {
	auto vertex{ GetShaderInfo(shader_pair.vertex, ShaderType::Vertex, program_name) };
	auto fragment{ GetShaderInfo(shader_pair.fragment, ShaderType::Fragment, program_name) };

	return { vertex, fragment };
}

ProgramInfo Shaders::GetProgramInfo(
	const std::variant<ShaderCode, ShaderPath>& code_or_path, std::string_view program_name
) const {
	auto [source_string, delete_after] = std::visit(
		[program_name]<typename T>(const T& arg) -> std::pair<std::string, bool> {
			if constexpr (std::is_same_v<T, ShaderPath>) {
				auto source{ FileToString(arg.path) };
				PTGN_ASSERT(
					HasVertexAndFragmentShader(source), "Shader program '", program_name,
					"' loaded from file must provide a vertex and fragment type: ", arg.path
				);
				return { source, arg.delete_after };
			} else if constexpr (std::is_same_v<T, ShaderCode>) {
				PTGN_ASSERT(
					HasVertexAndFragmentShader(arg.content), "Shader program '", program_name,
					"' loaded from code must provide a vertex and fragment type: ", arg.content
				);
				return { arg.content, arg.delete_after };
			} else {
				static_assert(false, "Incomplete visitor");
			}
		},
		code_or_path
	);

	auto srcs{ ParseShaderSourceFile(source_string, program_name) };

	PTGN_ASSERT(
		srcs.size() == 2,
		"Shader program file must provide a vertex and fragment type: ", program_name
	);

	const auto& first{ srcs[0] };
	const auto& second{ srcs[1] };

	std::string vertex_source;
	std::string fragment_source;

	if (first.type == ShaderType::Vertex && second.type == ShaderType::Fragment) {
		vertex_source	= first.code.content;
		fragment_source = second.code.content;
	} else if (first.type == ShaderType::Fragment && second.type == ShaderType::Vertex) {
		fragment_source = first.code.content;
		vertex_source	= second.code.content;
	} else {
		PTGN_ERROR("ShaderId file must provide a vertex and fragment type: ", program_name);
	}

	auto vertex_id{ CompileShader(ShaderType::Vertex, vertex_source) };
	auto fragment_id{ CompileShader(ShaderType::Fragment, fragment_source) };

	return ProgramInfo{ .vertex	  = { vertex_id, delete_after },
						.fragment = { fragment_id, delete_after } };
}

ProgramInfo Shaders::GetProgramInfo(
	const std::variant<ShaderCode, ShaderPath, ShaderPair>& variant, std::string_view program_name
) const {
	return std::visit(
		[this, program_name]<typename T>(const T& arg) -> ProgramInfo {
			if constexpr (std::is_same_v<T, ShaderPair>) {
				return GetProgramInfo(arg, program_name);
			} else if constexpr (std::is_same_v<T, ShaderCode> || std::is_same_v<T, ShaderPath>) {
				return GetProgramInfo(std::variant<ShaderCode, ShaderPath>{ arg }, program_name);
			} else {
				static_assert(false, "Incomplete visitor");
			}
		},
		variant
	);
}

ShaderId Shaders::CreateProgram(
	const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view program_name
) {
	auto program{ CreateProgram(program_name) };

	auto info{ GetProgramInfo(source, program_name) };

	LinkProgram(program, info.vertex.id, info.fragment.id);

	if (info.vertex.delete_after && info.vertex.id) {
		DeleteShaderId(info.vertex.id, ShaderType::Vertex);
	}

	if (info.fragment.delete_after && info.fragment.id) {
		DeleteShaderId(info.fragment.id, ShaderType::Fragment);
	}

	return program;
}

ShaderId Shaders::CreateProgram(std::string_view program_name) {
	ShaderId id{ GLCallReturn(glCreateProgram()) };
	PTGN_ASSERT(id, "Failed to create shader program");
	cache_.Add(id, ProgramCache{ .program_name = std::string{ program_name } });
	return id;
}

std::span<const std::string> Shaders::GetVertexShaderNames() const {
	return vertex_shader_names_;
}

std::span<const std::string> Shaders::GetFragmentShaderNames() const {
	return fragment_shader_names_;
}

void Shaders::DestroyProgram(ShaderId id) {
	if (!id) {
		return;
	}
	gl_.ForgetId(id);
	PTGN_ASSERT(!gl_.IsBound(id), "ShaderId must not be bound when destroying it");
	DeleteProgramId(id);
	cache_.Remove(id);
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, const Matrix4& v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		constexpr bool transpose_matrix{ false };
		constexpr int matrix_count{ 1 };
		GLCall(glUniformMatrix4fv(location, matrix_count, transpose_matrix, v.Data()));
	} else {
		PTGN_WARN("Shader location not found for uniform: ", uniform_name);
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, float v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform1f(location, v));
	} else {
		PTGN_WARN("Shader location not found for uniform: ", uniform_name);
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, V2_float v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform2f(location, v.x, v.y));
	} else {
		PTGN_WARN("Shader location not found for uniform: ", uniform_name);
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, V3_float v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform3f(location, v.x, v.y, v.z));
	} else {
		PTGN_WARN("Shader location not found for uniform: ", uniform_name);
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, V4_float v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform4f(location, v.x, v.y, v.z, v.w));
	} else {
		PTGN_WARN("Shader location not found for uniform: ", uniform_name);
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, std::span<const float> values) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform1fv(location, static_cast<std::int32_t>(values.size()), values.data()));
	} else {
		PTGN_WARN("Shader location not found for uniform: ", uniform_name);
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, int v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform1i(location, v));
	} else {
		PTGN_WARN("Shader location not found for uniform: ", uniform_name);
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, V2_int v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform2i(location, v.x, v.y));
	} else {
		PTGN_WARN("Shader location not found for uniform: ", uniform_name);
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, V3_int v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform3i(location, v.x, v.y, v.z));
	} else {
		PTGN_WARN("Shader location not found for uniform: ", uniform_name);
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, V4_int v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform4i(location, v.x, v.y, v.z, v.w));
	} else {
		PTGN_WARN("Shader location not found for uniform: ", uniform_name);
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, std::span<const int> v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform1iv(location, static_cast<std::int32_t>(v.size()), v.data()));
	} else {
		PTGN_WARN("Shader location not found for uniform: ", uniform_name);
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, bool v) {
	SetUniform(id, uniform_name, static_cast<std::int32_t>(v));
}

std::int32_t Shaders::GetUniform(ShaderId id, const char* uniform_name) {
	PTGN_ASSERT(
		gl_.IsBound(id),
		"Cannot get uniform location of shader program which is not currently bound"
	);
	PTGN_ASSERT(id, "Invalid shader id");

	auto& cache{ cache_.Get(id) };

	auto hash{ Hash(uniform_name) };

	if (auto location{ cache.uniform_locations.find(hash) };
		location != cache.uniform_locations.end()) {
		return location->second;
	}

	std::int32_t location{ GLCallReturn(glGetUniformLocation(id, uniform_name)) };

	cache.uniform_locations.emplace(hash, location);

	return location;
}

ShaderId Shaders::GetProgram(std::string_view program_name) const {
	auto hash{ Hash(program_name) };
	if (!programs_.contains(hash)) {
		PTGN_WARN("Shader program with name '", program_name, "' not found");
		return ShaderId{ 0 };
	}
	return programs_.find(hash)->second;
}

} // namespace ptgn::impl::gl
