#include "renderer/materials/shader.h"

#include <cmrc/cmrc.hpp>
#include <cstdint>
#include <filesystem>
#include <format>
#include <list>
#include <ostream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "core/app/game.h"
#include "core/util/file.h"
#include "core/util/span.h"
#include "debug/core/debug_config.h"
#include "debug/core/log.h"
#include "debug/runtime/assert.h"
#include "debug/runtime/debug_system.h"
#include "debug/runtime/stats.h"
#include "math/hash.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "renderer/gl/gl_helper.h"
#include "renderer/gl/gl_loader.h"
#include "renderer/render_data.h"
#include "renderer/renderer.h"
#include "serialization/json/fwd.h"

namespace ptgn {

namespace impl {

using Header = std::string;

static std::string TrimWhitespace(const std::string& s) {
	std::size_t start{ s.find_first_not_of(" \n\r\t") };
	if (start == std::string::npos) {
		return "";
	}
	std::size_t end{ s.find_last_not_of(" \n\r\t") };
	return s.substr(start, end - start + 1);
}

static ShaderType GetShaderType(const std::string& type) {
	if (type == "fragment") {
		return ShaderType::Fragment;
	} else if (type == "vertex") {
		return ShaderType::Vertex;
	}
	PTGN_ERROR("Unknown shader type: ", type);
}

// Extract just the content inside R"( ... )"
static void TrimRawStringLiteral(std::string& content) {
	const std::string raw_start{ "R\"(" };
	const std::string raw_end{ ")\"" };

	std::size_t start{ content.find(raw_start) };
	std::size_t end{ content.rfind(raw_end) };

	if (start != std::string::npos && end != std::string::npos &&
		end > start + raw_start.length()) {
		content = content.substr(start + raw_start.length(), end - (start + raw_start.length()));
	}
}

static std::pair<Header, std::vector<ShaderTypeSource>> ParseShaderSources(
	const std::string& source, const std::string& name_without_ext
) {
	Header header;
	std::vector<ShaderTypeSource> sources;

	std::string input{ source };
	TrimRawStringLiteral(input);

	const auto contains_type = [&sources](ShaderType type) {
		return VectorFindIf(sources, [type](const ShaderTypeSource& sts) {
			return sts.type == type;
		});
	};

	// Regex to find: #type <stage> and capture everything until next #type or EOF
	std::regex type_regex(R"(#type\s+(\w+))");
	auto words_begin{ std::sregex_iterator(input.begin(), input.end(), type_regex) };
	auto words_end{ std::sregex_iterator() };

	std::vector<std::pair<std::string, std::size_t>> found_types; // (type, position)

	for (auto i{ words_begin }; i != words_end; ++i) {
		std::smatch match{ *i };
		std::string type{ match[1].str() };
		std::size_t pos{ static_cast<std::size_t>(match.position()) };
		found_types.emplace_back(type, pos);
	}

	PTGN_ASSERT(
		!found_types.empty(), "No #type declarations found in shader source: ", name_without_ext
	);

	// Extract header before the first #type
	std::size_t first_type_pos{ found_types.front().second };
	std::string header_code{ input.substr(0, first_type_pos) };
	header = TrimWhitespace(header_code);

	// Extract blocks between #type markers
	for (std::size_t i = 0; i < found_types.size(); i++) {
		auto type_string{ found_types[i].first };
		auto type{ GetShaderType(type_string) };
		std::size_t start{ found_types[i].second + std::string("#type ").size() +
						   type_string.size() };

		std::size_t end{ input.size() };

		if (i + 1 < found_types.size()) {
			end = found_types[i + 1].second;
		}

		std::string code{ input.substr(start, end - start) };
		code = TrimWhitespace(code);

		PTGN_ASSERT(!contains_type(type), "GLSL file can only contain one type of shader: ", type);

		sources.emplace_back(type, ShaderCode{ code }, name_without_ext);
	}

	return { header, sources };
}

static bool HasOption(const std::string& string, const std::string& option_name) {
	return string.find("#option " + option_name) != std::string::npos;
}

static void RemoveOption(std::string& source, const std::string& option = "") {
	// @param option Default: Removes all options in source.
	std::regex pattern;

	if (option.empty()) {
		// Remove ALL `#option <something>` lines (case-insensitive)
		pattern = std::regex(R"(^\s*#option\s+\w+\s*\n?)", std::regex::icase);
	} else {
		// Remove only specific `#option <option>` lines (case-insensitive)
		pattern = std::regex(R"(^\s*#option\s+)" + option + R"(\s*\n?)", std::regex::icase);
	}

	source = std::regex_replace(source, pattern, "");
}

static std::string InjectShaderPreamble(
	const std::string& source, [[maybe_unused]] ShaderType type
) {
	std::string result{ source };

	std::regex version_regex(R"(#version\s+(\d+)(?:\s+(\w+))?)");
	std::smatch match;

	if (std::regex_search(source, match, version_regex)) {
		std::string version_number{ match[1].str() };		  // e.g. "330" or "300"
		std::string version_profile{ match.size() > 2 ? match[2].str()
													  : "" }; // e.g. "core" or "es"

#ifdef __EMSCRIPTEN__
		PTGN_ASSERT(
			version_number == "300" && version_profile == "es",
			"For Emscripten, shader must specify '#version 300 es'"
		);
#else
		PTGN_ASSERT(
			version_number == "330" && version_profile == "core",
			"For desktop, shader must specify '#version 330 core'"
		);
#endif
	} else {
#ifdef __EMSCRIPTEN__
		// Automatically add version directive.
		result = "#version 300 es\n" + result;
#else
		result = "#version 330 core\n" + result;
#endif
	}

	// Insert after #version line
	size_t version_line_end{ result.find('\n') };
	size_t insert_pos{ (version_line_end != std::string::npos) ? version_line_end + 1
															   : result.size() };

#ifdef __EMSCRIPTEN__
	// Inject precision (only for on Emscripten)
	std::regex precision_regex(R"(precision\s+(highp|mediump|lowp)\s+float\s*;)");
	if (!std::regex_search(result, precision_regex)) {
		std::string precision{ "precision highp float;\n" };
		result.insert(insert_pos, precision);
		// insert_pos += extension.length(); // Update insert position.
	}
#else
	// Inject #extension if needed (desktop only)
	if (result.find("#extension GL_ARB_separate_shader_objects") == std::string::npos) {
		std::string extension{ "#extension GL_ARB_separate_shader_objects : require\n" };
		result.insert(insert_pos, extension);
		// insert_pos += extension.length(); // Update insert position.
	}
#endif

	return result;
}

static void AddShaderLayout(std::string& source, [[maybe_unused]] ShaderType type) {
	std::string result;

	std::istringstream input{ source };
	std::ostringstream output;

	std::string line;
	bool in_main{ false };
	int current_in_location{ 0 };
	int current_out_location{ 0 };

	// Matches GLSL input/output variable declarations like:
	//    in vec3 position;
	//    out vec4 o_Color;
	// The pattern explained:
	// ^\s*                      - Start of line with optional leading whitespace
	// (in|out)                  - Capture group 1: either 'in' or 'out'
	// \s+                       - One or more spaces after 'in' or 'out'
	// [a-zA-Z_][a-zA-Z0-9_]*    - Capture group 2: type name (e.g., vec3, float), must start
	// with a letter or underscore
	// \s+                       - One or more spaces after type
	// [a-zA-Z_][a-zA-Z0-9_]*    - Capture group 3: variable name (e.g., a_Position, o_Color),
	// valid identifier
	// \s*;                      - Optional spaces before semicolon, then a required semicolon
	// \r?                       - Match zero or one carriage return character
	// $                         - Match string end
	std::regex var_decl_regex(
		R"(^\s*(in|out)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*;\r?$)"
	);

	std::smatch match;

	std::regex layout_regex(R"(layout\s*\(\s*location\s*=\s*\d+\s*\))");

	while (std::getline(input, line)) {
		// Stop injecting once we hit `void main()`
		if (!in_main && line.find("void main") != std::string::npos) {
			in_main = true;
		}

		if (in_main) {
			output << line << "\n";
			continue;
		}

		PTGN_ASSERT(
			!std::regex_search(line, layout_regex),
			"Cannot use #option auto_layout and define a custom attribute layout: ", line
		);

		if (!std::regex_match(line, match, var_decl_regex)) {
			output << line << "\n";
			continue;
		}

		bool inject_layout{ true };

		std::string qualifier{ match[1].str() }; // "in" or "out"

#ifdef __EMSCRIPTEN__
		// Only inject layout for Vertex Shader & 'in' variables on WebAssembly
		if (!(type == ShaderType::Vertex && qualifier == "in")) {
			inject_layout = false;
		}
#endif

		if (inject_layout) {
			std::string variable_type{ match[2].str() }; // (e.g., vec3)
			std::string variable_name{ match[3].str() }; // (e.g., a_Position)

			int location{ (qualifier == "in") ? current_in_location++ : current_out_location++ };

			std::string layout_line{ std::format(
				"layout(location = {}) {} {} {};", location, qualifier, variable_type, variable_name
			) };

			output << layout_line << "\n";
			continue;
		}

		output << line << "\n";
	}

	source = output.str();
}

static std::string GenerateTextureSwitchBlock(std::size_t max_texture_slots) {
	std::ostringstream oss;
	for (std::size_t i{ 0 }; i < max_texture_slots; ++i) {
		oss << std::format(
			"    if (v_TexIndex == {}.0f) {{\n"
			"        texColor *= texture(u_Texture[{}], v_TexCoord);\n"
			"    }}\n",
			i, i
		);
	}
	return oss.str();
}

static std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
	if (from.empty()) {
		return str;
	}

	std::size_t start_pos{ 0 };
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // Move past the replacement
	}
	return str;
}

static std::vector<ShaderTypeSource> ParseShader(
	const std::string& source, const std::string& name_without_ext
) {
	std::vector<ShaderTypeSource> output;

	auto [header, sources] = ParseShaderSources(source, name_without_ext);

	// PTGN_LOG("-------- Name ---------");
	// PTGN_LOG(name_without_ext);
	// PTGN_LOG("------- Header ---------");
	// PTGN_LOG(header);

	auto auto_layout_name{ "auto_layout" };

	bool global_auto_layout{ HasOption(header, auto_layout_name) };

	for (std::size_t i{ 0 }; i < sources.size(); i++) {
		auto& sts{ sources[i] };
		if (global_auto_layout || HasOption(sts.source.source_, auto_layout_name)) {
			AddShaderLayout(sts.source.source_, sts.type);
		}
		RemoveOption(sts.source.source_);
		sts.source.source_ = InjectShaderPreamble(sts.source.source_, sts.type);
		output.emplace_back(sts);
		// PTGN_LOG("------- Source ", i, " (type: ", sts.type, ") -------------");
		// PTGN_LOG(sts.source.source_);
	}
	return output;
}

static void CompileShaders(const std::vector<ShaderTypeSource>& sources, ShaderCache& cache) {
	for (const auto& sts : sources) {
		auto hash{ Hash(sts.name) };
		auto shader_id{ Shader::Compile(sts.type, sts.source.source_) };
		switch (sts.type) {
			case ShaderType::Fragment:
				PTGN_ASSERT(
					!cache.fragment_shaders.contains(hash),
					"Cannot add shader to cache twice: ", sts.name
				);
				cache.fragment_shaders.try_emplace(hash, shader_id);
				break;
			case ShaderType::Vertex:
				PTGN_ASSERT(
					!cache.vertex_shaders.contains(hash),
					"Cannot add shader to cache twice: ", sts.name
				);
				cache.vertex_shaders.try_emplace(hash, shader_id);
				break;
			default: PTGN_ERROR("Unknown shader type")
		}
	}
}

static void SubstituteShaderTokens(
	std::vector<ShaderTypeSource>& sources, std::size_t max_texture_slots
) {
	// This is primarily for the quad shader, which requires a block of if-statements based on
	// how many texture slots there are.

	std::string switch_block{ GenerateTextureSwitchBlock(max_texture_slots) };
	auto slots{ std::to_string(max_texture_slots) };

	for (auto& sts : sources) {
		sts.source.source_ = ReplaceAll(sts.source.source_, "{MAX_TEXTURE_SLOTS}", slots);
		sts.source.source_ = ReplaceAll(sts.source.source_, "{TEXTURE_SWITCH_BLOCK}", switch_block);
	}
}

static void PopulateShaderCache(
	const cmrc::embedded_filesystem& fs, ShaderCache& cache, std::size_t max_texture_slots
) {
	std::string subdir{ "common/" };
	auto dir{ fs.iterate_directory(subdir) };

	std::vector<ShaderTypeSource> sources;

	for (auto resource : dir) {
		if (!resource.is_file()) {
			continue;
		}
		auto filename{ resource.filename() };
		auto file{ fs.open(subdir + filename) };
		std::string shader_src(file.begin(), file.end());
		std::string name_without_ext{ std::filesystem::path(filename).stem().string() };
		auto srcs{ ParseShader(shader_src, name_without_ext) };
		sources.insert(sources.end(), srcs.begin(), srcs.end());
	}

	SubstituteShaderTokens(sources, max_texture_slots);
	CompileShaders(sources, cache);
}

static json GetManifest(const cmrc::embedded_filesystem& fs) {
	std::string manifest_name{ "manifest.json" };

	PTGN_ASSERT(
		fs.exists(manifest_name), "Could not find shader manifest file with name: ", manifest_name
	);
	auto manifest_file{ fs.open(manifest_name) };

	std::string_view manifest_data(manifest_file.begin(), manifest_file.end());

	json manifest = json::parse(manifest_data);

	// PTGN_LOG("--------- Manifest Name ----------");
	// PTGN_LOG(manifest_name);
	// PTGN_LOG("-------- Manifest Content -------");
	// PTGN_LOG(manifest.dump(4));
	return manifest;
}

void ShaderManager::PopulateShadersFromCache(const json& manifest) {
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
				"property for same-name vertex/fragment shaders"
			);
		}

		auto vert_hash{ Hash(vertex_name) };
		auto frag_hash{ Hash(fragment_name) };

		PTGN_ASSERT(
			cache_.vertex_shaders.contains(vert_hash), "Vertex shader: ", vertex_name, " for ",
			shader_name, " not found in shader directory"
		);

		PTGN_ASSERT(
			cache_.fragment_shaders.contains(frag_hash), "Fragment shader: ", fragment_name,
			" for ", shader_name, " not found in shader directory"
		);

		auto vert_it{ cache_.vertex_shaders.find(vert_hash) };
		auto frag_it{ cache_.fragment_shaders.find(frag_hash) };

		ShaderId vert_id{ vert_it->second };
		ShaderId frag_id{ frag_it->second };

		auto hash{ Hash(shader_name) };

		shaders_.emplace(hash, Shader{ vert_id, frag_id, shader_name });
	}
}

const Shader& ShaderManager::TryLoad(
	std::string_view shader_name, std::variant<ShaderCode, path> source
) {
	auto hash{ Hash(shader_name) };
	auto [it, _] = shaders_.try_emplace(hash, source, std::string(shader_name));
	return it->second;
}

const Shader& ShaderManager::TryLoad(
	std::string_view shader_name, std::variant<ShaderCode, std::string> vertex,
	std::variant<ShaderCode, std::string> fragment
) {
	auto hash{ Hash(shader_name) };
	auto [it, _] = shaders_.try_emplace(hash, vertex, fragment, std::string(shader_name));
	return it->second;
}

const Shader& ShaderManager::Get(std::string_view shader_name) const {
	auto hash{ Hash(shader_name) };
	PTGN_ASSERT(
		shaders_.contains(hash), "Shader with name: ", shader_name, " not found in shader manager"
	);
	auto shader_it{ shaders_.find(hash) };
	return shader_it->second;
}

ShaderId ShaderManager::Get(ShaderType type, std::string_view shader_name) const {
	auto hash{ Hash(shader_name) };
	PTGN_ASSERT(
		Has(type, shader_name), "Could not find ", type, " shader with name: ", shader_name
	);
	switch (type) {
		case ShaderType::Fragment: {
			auto it{ cache_.fragment_shaders.find(hash) };
			return it->second;
		};
		case ShaderType::Vertex: {
			auto it{ cache_.vertex_shaders.find(hash) };
			return it->second;
		};
		default: PTGN_ERROR("Unknown shader type")
	}
}

bool ShaderManager::Has(std::string_view shader_name) const {
	auto hash{ Hash(shader_name) };
	return shaders_.contains(hash);
}

bool ShaderManager::Has(ShaderType type, std::string_view shader_name) const {
	auto hash{ Hash(shader_name) };
	switch (type) {
		case ShaderType::Fragment: {
			return cache_.fragment_shaders.contains(hash);
		};
		case ShaderType::Vertex: {
			return cache_.vertex_shaders.contains(hash);
		};
		default: PTGN_ERROR("Unknown shader type")
	}
}

void ShaderManager::Init() {
	std::size_t max_texture_slots{ game.renderer.render_data_.GetMaxTextureSlots() };

	PTGN_ASSERT(max_texture_slots > 0, "Max texture slots must be set before initializing shaders");

	PTGN_INFO("Renderer Texture Slots: ", max_texture_slots);

	auto fs{ cmrc::shader::get_filesystem() };

	PopulateShaderCache(fs, cache_, max_texture_slots);

	auto manifest = GetManifest(fs);

	PopulateShadersFromCache(manifest);
}

void ShaderManager::Shutdown() {
	const auto delete_shaders = [](const auto& container) {
		for (const auto& [_, id] : container) {
			if (id) {
				GLCall(DeleteShader(id));
			}
		}
	};

	delete_shaders(cache_.vertex_shaders);
	delete_shaders(cache_.fragment_shaders);
}

std::vector<ShaderTypeSource> ShaderManager::ParseShaderSourceFile(
	const std::string& source, const std::string& name
) {
	auto srcs{ ParseShader(source, name) };
	SubstituteShaderTokens(srcs, game.renderer.render_data_.GetMaxTextureSlots());
	return srcs;
}

ShaderId CompileSource(const std::string& source, ShaderType type, const std::string& name) {
	auto srcs{ ShaderManager::ParseShaderSourceFile(source, name) };
	PTGN_ASSERT(srcs.size() == 1, "Wrong constructor for a multi-source shader file");
	const auto& front{ srcs.front() };
	PTGN_ASSERT(front.type == type, "Shader type mismatch");
	return Shader::Compile(type, front.source.source_);
}

} // namespace impl

static ShaderId CompilePath(const path& p, ShaderType type, const std::string& name) {
	PTGN_ASSERT(FileExists(p), "Cannot create shader from nonexistent shader path: ", p.string());
	auto source{ FileToString(p) };
	return impl::CompileSource(source, type, name);
}

Shader::Shader(ShaderId vertex, ShaderId fragment, const std::string& shader_name) :
	shader_name_{ shader_name } {
	Create();

	Link(vertex, fragment);
}

Shader::Shader(std::variant<ShaderCode, path> source, const std::string& shader_name) :
	shader_name_{ shader_name } {
	Create();

	std::string source_string;

	if (std::holds_alternative<path>(source)) {
		const auto& p{ std::get<path>(source) };
		source_string = FileToString(p);
	} else if (std::holds_alternative<ShaderCode>(source)) {
		const auto& src{ std::get<ShaderCode>(source) };
		source_string = src.source_;
	} else {
		PTGN_ERROR("Unknown variant type");
	}

	auto srcs{ impl::ShaderManager::ParseShaderSourceFile(source_string, shader_name) };

	PTGN_ASSERT(
		srcs.size() == 2, "Shader file must provide a vertex and fragment type: ", shader_name
	);

	const auto& first{ srcs[0] };
	const auto& second{ srcs[1] };

	std::string vertex_source;
	std::string fragment_source;

	if (first.type == ShaderType::Vertex && second.type == ShaderType::Fragment) {
		vertex_source	= first.source.source_;
		fragment_source = second.source.source_;
	} else if (first.type == ShaderType::Fragment && second.type == ShaderType::Vertex) {
		fragment_source = first.source.source_;
		vertex_source	= second.source.source_;
	} else {
		PTGN_ERROR("Shader file must provide a vertex and fragment type: ", shader_name);
	}

	ShaderId vertex_id{ Shader::Compile(ShaderType::Vertex, vertex_source) };
	ShaderId fragment_id{ Shader::Compile(ShaderType::Fragment, fragment_source) };

	Link(vertex_id, fragment_id);

	if (vertex_id) {
		GLCall(DeleteShader(vertex_id));
	}

	if (fragment_id) {
		GLCall(DeleteShader(fragment_id));
	}
}

Shader::Shader(
	std::variant<ShaderCode, std::string> vertex, std::variant<ShaderCode, std::string> fragment,
	const std::string& shader_name
) :
	shader_name_{ shader_name } {
	// bool: If true, delete shader id after.
	const auto get_id = [shader_name](
							const std::variant<ShaderCode, std::string>& v, ShaderType type
						) -> std::pair<ShaderId, bool> {
		if (std::holds_alternative<std::string>(v)) {
			const auto& name{ std::get<std::string>(v) };
			path file{ name };
			if (FileExists(file)) {
				PTGN_ASSERT(
					file.extension() == ".glsl",
					"Shader file extension must be .glsl: ", file.string()
				);
				return { CompilePath(file, type, shader_name), true };
			} else if (game.shader.Has(type, name)) {
				return { game.shader.Get(type, name), false };
			} else {
				PTGN_ERROR(name, " is not a valid shader path or loaded ", type, " shader name");
			}
		} else if (std::holds_alternative<ShaderCode>(v)) {
			const auto& src{ std::get<ShaderCode>(v) };
			return { impl::CompileSource(src.source_, type, shader_name), true };
		} else {
			PTGN_ERROR("Unknown variant type");
		}
	};

	Create();

	auto [vertex_id, delete_vert_after]	  = get_id(vertex, ShaderType::Vertex);
	auto [fragment_id, delete_frag_after] = get_id(fragment, ShaderType::Fragment);

	Link(vertex_id, fragment_id);

	if (delete_vert_after && vertex_id) {
		GLCall(DeleteShader(vertex_id));
	}

	if (delete_frag_after && fragment_id) {
		GLCall(DeleteShader(fragment_id));
	}
}

Shader::Shader(Shader&& other) noexcept :
	id_{ std::exchange(other.id_, 0) },
	shader_name_{ std::exchange(other.shader_name_, {}) },
	location_cache_{ std::exchange(other.location_cache_, {}) } {}

Shader& Shader::operator=(Shader&& other) noexcept {
	if (this != &other) {
		Delete();
		id_				= std::exchange(other.id_, 0);
		shader_name_	= std::exchange(other.shader_name_, {});
		location_cache_ = std::exchange(other.location_cache_, {});
	}
	return *this;
}

Shader::~Shader() {
	Delete();
}

void Shader::Create() {
	id_ = GLCallReturn(CreateProgram());
	PTGN_ASSERT(IsValid(), "Failed to create shader program using OpenGL context");
#ifdef GL_ANNOUNCE_SHADER_CALLS
	PTGN_LOG("GL: Created shader program with id ", id_);
#endif
}

void Shader::Delete() noexcept {
	if (!IsValid()) {
		return;
	}
	GLCall(DeleteProgram(id_));
#ifdef GL_ANNOUNCE_SHADER_CALLS
	PTGN_LOG("GL: Deleted shader program with id ", id_);
#endif
	id_ = 0;
}

ShaderId Shader::Compile(ShaderType type, const std::string& source) {
	ShaderId id{ GLCallReturn(CreateShader(static_cast<std::uint32_t>(type))) };

	auto src{ source.c_str() };

	GLCall(ShaderSource(id, 1, &src, nullptr));
	GLCall(CompileShader(id));

	// Check for shader compilation errors.
	std::int32_t result{ GL_FALSE };
	GLCall(GetShaderiv(id, GL_COMPILE_STATUS, &result));

	if (result == GL_FALSE) {
		std::int32_t length{ 0 };
		GLCall(GetShaderiv(id, GL_INFO_LOG_LENGTH, &length));
		std::string log;
		log.resize(static_cast<std::size_t>(length));
		GLCall(GetShaderInfoLog(id, length, &length, &log[0]));

		GLCall(DeleteShader(id));

		PTGN_ERROR("Failed to compile ", type, " shader: \n", source, "\n", log);
	}

	return id;
}

void Shader::Link(ShaderId vertex, ShaderId fragment) {
	location_cache_.clear();

	PTGN_ASSERT(vertex);
	PTGN_ASSERT(fragment);

	GLCall(AttachShader(id_, vertex));
	GLCall(AttachShader(id_, fragment));
	GLCall(LinkProgram(id_));

	// Check for shader link errors.
	std::int32_t linked{ GL_FALSE };
	GLCall(GetProgramiv(id_, GL_LINK_STATUS, &linked));

	if (linked == GL_FALSE) {
		std::int32_t length{ 0 };
		GLCall(GetProgramiv(id_, GL_INFO_LOG_LENGTH, &length));
		std::string log;
		log.resize(static_cast<std::size_t>(length));
		GLCall(GetProgramInfoLog(id_, length, &length, &log[0]));

		GLCall(DeleteProgram(id_));

		GLCall(DeleteShader(vertex));
		GLCall(DeleteShader(fragment));

		PTGN_ERROR(
			"Failed to link shaders to program: \nVertex: ", vertex, "\nFragment: ", fragment, "\n",
			log
		);
	}

	GLCall(ValidateProgram(id_));
}

void Shader::Compile(const std::string& vertex_source, const std::string& fragment_source) {
	location_cache_.clear();

	ShaderId vertex{ Compile(ShaderType::Vertex, vertex_source) };
	ShaderId fragment{ Compile(ShaderType::Fragment, fragment_source) };

	if (vertex && fragment) {
		GLCall(AttachShader(id_, vertex));
		GLCall(AttachShader(id_, fragment));
		GLCall(LinkProgram(id_));

		// Check for shader link errors.
		std::int32_t linked{ GL_FALSE };
		GLCall(GetProgramiv(id_, GL_LINK_STATUS, &linked));

		if (linked == GL_FALSE) {
			std::int32_t length{ 0 };
			GLCall(GetProgramiv(id_, GL_INFO_LOG_LENGTH, &length));
			std::string log;
			log.resize(static_cast<std::size_t>(length));
			GLCall(GetProgramInfoLog(id_, length, &length, &log[0]));

			GLCall(DeleteProgram(id_));

			GLCall(DeleteShader(vertex));
			GLCall(DeleteShader(fragment));

			PTGN_ERROR(
				"Failed to link shaders to program: \n", vertex_source, "\n", fragment_source, "\n",
				log
			);
		}

		GLCall(ValidateProgram(id_));
	}

	if (vertex) {
		GLCall(DeleteShader(vertex));
	}

	if (fragment) {
		GLCall(DeleteShader(fragment));
	}
}

void Shader::Bind(ShaderId id) {
	if (game.renderer.bound_.shader_id == id) {
		return;
	}
	GLCall(UseProgram(id));
	game.renderer.bound_.shader_id = id;
#ifdef PTGN_DEBUG
	++game.debug.stats.shader_binds;
#endif
#ifdef GL_ANNOUNCE_SHADER_CALLS
	PTGN_LOG("GL: Bound shader program with id ", id);
#endif
}

void Shader::Bind() const {
	PTGN_ASSERT(IsValid(), "Cannot bind destroyed or uninitialized shader");
	Bind(id_);
}

bool Shader::IsBound() const {
	return GetBoundId() == id_;
}

ShaderId Shader::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(glGetIntegerv(GL_CURRENT_PROGRAM, &id));
	PTGN_ASSERT(id >= 0, "Failed to retrieve bound shader id");
	return static_cast<ShaderId>(id);
}

std::int32_t Shader::GetUniform(const std::string& name) const {
	PTGN_ASSERT(IsBound(), "Cannot get uniform location of shader which is not currently bound");
	if (auto it{ location_cache_.find(name) }; it != location_cache_.end()) {
		return it->second;
	}

	std::int32_t location{ GLCallReturn(GetUniformLocation(id_, name.c_str())) };

	location_cache_.try_emplace(name, location);
	return location;
}

void Shader::SetUniform(const std::string& name, const Vector2<float>& v) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform2f(location, v.x, v.y));
	}
}

void Shader::SetUniform(const std::string& name, const Vector3<float>& v) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform3f(location, v.x, v.y, v.z));
	}
}

void Shader::SetUniform(const std::string& name, const Vector4<float>& v) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform4f(location, v.x, v.y, v.z, v.w));
	}
}

void Shader::SetUniform(const std::string& name, const Matrix4& matrix) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(UniformMatrix4fv(location, 1, GL_FALSE, matrix.Data()));
	}
}

void Shader::SetUniform(const std::string& name, const std::int32_t* data, std::int32_t count)
	const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform1iv(location, count, data));
	}
}

void Shader::SetUniform(const std::string& name, const float* data, std::int32_t count) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform1fv(location, count, data));
	}
}

void Shader::SetUniform(const std::string& name, const Vector2<std::int32_t>& v) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform2i(location, v.x, v.y));
	}
}

void Shader::SetUniform(const std::string& name, const Vector3<std::int32_t>& v) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform3i(location, v.x, v.y, v.z));
	}
}

void Shader::SetUniform(const std::string& name, const Vector4<std::int32_t>& v) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform4i(location, v.x, v.y, v.z, v.w));
	}
}

void Shader::SetUniform(const std::string& name, float v0) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform1f(location, v0));
	}
}

void Shader::SetUniform(const std::string& name, float v0, float v1) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform2f(location, v0, v1));
	}
}

void Shader::SetUniform(const std::string& name, float v0, float v1, float v2) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform3f(location, v0, v1, v2));
	}
}

void Shader::SetUniform(const std::string& name, float v0, float v1, float v2, float v3) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform4f(location, v0, v1, v2, v3));
	}
}

void Shader::SetUniform(const std::string& name, std::int32_t v0) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform1i(location, v0));
	}
}

void Shader::SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform2i(location, v0, v1));
	}
}

void Shader::SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2)
	const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform3i(location, v0, v1, v2));
	}
}

void Shader::SetUniform(
	const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2, std::int32_t v3
) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform4i(location, v0, v1, v2, v3));
	}
}

void Shader::SetUniform(const std::string& name, bool value) const {
	SetUniform(name, static_cast<std::int32_t>(value));
}

bool Shader::IsValid() const {
	return id_;
}

ShaderId Shader::GetId() const {
	return id_;
}

std::string_view Shader::GetName() const {
	return shader_name_;
}

} // namespace ptgn