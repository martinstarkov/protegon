#include "renderer/backend/gl/gl_shader.h"

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
#include "core/util/span.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_debug.h"
#include "renderer/primitives/shader.h"
#include "serialization/json/json.h"

namespace ptgn::impl::gl {

using Header = std::string;

static void DeleteShaderId(ShaderId id, ShaderType type) {
	GLCall(DeleteShader(id));
#ifdef PTGN_GL_DEBUG_SHADERS
	PTGN_LOG("glDeleteShader(type=", type, ",id=", id, ")");
#endif
}

static void DeleteProgramId(ShaderId id) {
	GLCall(DeleteProgram(id));
#ifdef PTGN_GL_DEBUG_SHADERS
	PTGN_LOG("glDeleteProgram(id=", id, ")");
#endif
}

static void LinkProgramId(ShaderId id) {
	GLCall(::LinkProgram(id));
#ifdef PTGN_GL_DEBUG_SHADERS
	PTGN_LOG("glLinkProgram(id=", id, ")");
#endif
}

static std::string_view TrimWhitespace(std::string_view s) {
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

static std::pair<Header, std::vector<ShaderSpec>> ParseShaderSources(
	const std::string& source, std::string_view name_without_ext
) {
	Header header;
	std::vector<ShaderSpec> sources;

	std::string input{ source };
	TrimRawStringLiteral(input);

	const auto contains_type = [&sources](auto type) {
		return VectorFindIf(sources, [type](const ShaderSpec& sts) { return sts.type == type; });
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

		sources.emplace_back(type, ShaderCode{ code }, std::string{ name_without_ext });
	}

	return { header, sources };
}

static bool HasOption(std::string_view string, const std::string& option_name) {
	return string.contains("#option " + option_name);
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

	if (std::smatch match; std::regex_search(source, match, version_regex)) {
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
	if (!result.contains("#extension GL_ARB_separate_shader_objects")) {
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
		if (!in_main && line.contains("void main")) {
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
		// Only inject layout for Vertex ShaderId & 'in' variables on WebAssembly
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
			"        texColor *= texture(u_Textures[{}], v_TexCoord);\n"
			"    }}\n",
			i, i
		);
	}
	return oss.str();
}

static std::string ReplaceAll(std::string str, std::string_view from, std::string_view to) {
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

static std::vector<ShaderSpec> ParseShader(
	const std::string& source, std::string_view name_without_ext
) {
	std::vector<ShaderSpec> output;

	auto [header, sources] = ParseShaderSources(source, name_without_ext);

	// PTGN_LOG("-------- Name ---------");
	// PTGN_LOG(name_without_ext);
	// PTGN_LOG("------- Header ---------");
	// PTGN_LOG(header);

	ShaderOptions global_options;
	global_options.auto_layout = HasOption(header, "auto_layout");

	for (std::size_t i{ 0 }; i < sources.size(); i++) {
		auto& sts{ sources[i] };
		sts.options = global_options;

		auto& src{ sts.code.content };

		sts.options.auto_layout |= HasOption(src, "auto_layout");

		if (sts.options.auto_layout) {
			AddShaderLayout(src, sts.type);
		}

		RemoveOption(src);

		src = InjectShaderPreamble(src, sts.type);
		output.emplace_back(sts);

		// PTGN_LOG("------- Source ", i, " (type: ", sts.type, ") -------------");
		// PTGN_LOG(src);
	}
	return output;
}

ShaderId Shaders::CompileShader(ShaderType type, const std::string& source) const {
	ShaderId id{ GLCallReturn(::CreateShader(std::to_underlying(type))) };
#ifdef PTGN_GL_DEBUG_SHADERS
	PTGN_LOG("glCreateShader(type=", type, ") -> id=", id);
#endif

	auto src{ source.c_str() };

	GLCall(ShaderSource(id, 1, &src, nullptr));
	GLCall(::CompileShader(id));

	// Check for shader compilation errors.
	std::int32_t result{ GL_FALSE };
	GLCall(GetShaderiv(id, GL_COMPILE_STATUS, &result));

	if (result == GL_FALSE) {
		std::int32_t length{ 0 };
		GLCall(GetShaderiv(id, GL_INFO_LOG_LENGTH, &length));
		std::string log;
		log.resize(static_cast<std::size_t>(length));
		GLCall(GetShaderInfoLog(id, length, &length, &log[0]));

		DeleteShaderId(id, type);

		PTGN_ERROR("Failed to compile ", type, " shader: \n", source, "\n", log);
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
				break;
			case ShaderType::Vertex:
				PTGN_ASSERT(
					!vertex_shaders_.contains(hash), "Cannot add shader to cache twice: ", sts.name
				);
				vertex_shaders_.emplace(hash, shader_id);
				break;
			default: PTGN_ERROR("Unknown shader type");
		}
	}
}

static void SubstituteShaderTokens(
	std::vector<ShaderSpec>& sources, std::size_t max_texture_slots
) {
	// This is primarily for the quad shader, which requires a block of if-statements based on
	// how many texture slots there are.

	PTGN_ASSERT(max_texture_slots > 0, "Cannot substitute shader tokens for 0 texture slots");

	std::string switch_block{ GenerateTextureSwitchBlock(max_texture_slots) };
	auto slots{ std::to_string(max_texture_slots) };

	for (auto& sts : sources) {
		sts.code.content = ReplaceAll(sts.code.content, "{MAX_TEXTURE_SLOTS}", slots);
		sts.code.content = ReplaceAll(sts.code.content, "{TEXTURE_SWITCH_BLOCK}", switch_block);
	}
}

void Shaders::PopulateShaderCache(const cmrc::embedded_filesystem& filesystem) {
	path subdir{ "common/" };
	auto dir{ filesystem.iterate_directory(subdir.string()) };

	std::vector<ShaderSpec> sources;

	for (const auto& resource : dir) {
		if (!resource.is_file()) {
			continue;
		}

		path filename{ resource.filename() };
		auto file{ filesystem.open((subdir / filename).string()) };
		std::string shader_src(file.begin(), file.end());
		std::string name_without_ext{ filename.stem().string() };
		auto srcs{ ParseShader(shader_src, name_without_ext) };
		sources.insert(sources.end(), srcs.begin(), srcs.end());
	}

	SubstituteShaderTokens(sources, max_texture_slots_);
	CompileShaders(sources);
}

static json GetShaderManifest(const cmrc::embedded_filesystem& fs) {
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
	auto srcs{ ParseShader(source, name) };
	SubstituteShaderTokens(srcs, max_texture_slots_);
	return srcs;
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

ShaderId Shaders::CompileShaderPath(const path& shader_path, ShaderType type, std::string_view name)
	const {
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

	GLCall(AttachShader(id, vertex));
	GLCall(AttachShader(id, fragment));

	LinkProgramId(id);

	// Check for shader link errors.
	std::int32_t linked{ GL_FALSE };
	GLCall(GetProgramiv(id, GL_LINK_STATUS, &linked));

	if (linked == GL_FALSE) {
		std::int32_t length{ 0 };
		GLCall(GetProgramiv(id, GL_INFO_LOG_LENGTH, &length));
		std::string log;
		log.resize(static_cast<std::size_t>(length));
		GLCall(GetProgramInfoLog(id, length, &length, &log[0]));

		DeleteProgramId(id);

		DeleteShaderId(vertex, ShaderType::Vertex);
		DeleteShaderId(fragment, ShaderType::Fragment);

		PTGN_ERROR(
			"Failed to link shaders to program:\nVertex : ", vertex, "\nFragment : ", fragment,
			"\n ", log
		);
	}

	GLCall(ValidateProgram(id));
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
		GLCall(AttachShader(id, vertex));
		GLCall(AttachShader(id, fragment));
		LinkProgramId(id);

		// Check for shader link errors.
		std::int32_t linked{ GL_FALSE };
		GLCall(GetProgramiv(id, GL_LINK_STATUS, &linked));

		if (linked == GL_FALSE) {
			std::int32_t length{ 0 };
			GLCall(GetProgramiv(id, GL_INFO_LOG_LENGTH, &length));
			std::string log;
			log.resize(static_cast<std::size_t>(length));
			GLCall(GetProgramInfoLog(id, length, &length, &log[0]));

			DeleteProgramId(id);

			DeleteShaderId(vertex, ShaderType::Vertex);
			DeleteShaderId(fragment, ShaderType::Fragment);

			PTGN_ERROR(
				"Failed to link shaders to program: \n", vertex_source, "\n", fragment_source, "\n",
				log
			);
		}

		GLCall(ValidateProgram(id));
	}

	if (vertex) {
		DeleteShaderId(vertex, ShaderType::Vertex);
	}

	if (fragment) {
		DeleteShaderId(fragment, ShaderType::Fragment);
	}
}

Shaders::Shaders(GLContext& gl) : gl_{ gl } {}

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

void Shaders::Populate(std::size_t max_texture_slots) {
	PTGN_ASSERT(max_texture_slots > 0, "Platform must support at least one texture slot");
	max_texture_slots_ = max_texture_slots;

	auto fs{ cmrc::shader::get_filesystem() };

	PopulateShaderCache(fs);

	auto manifest(GetShaderManifest(fs));

	PopulateShadersFromCache(manifest);
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

std::pair<ShaderId, bool> Shaders::GetShaderIdWithDeleteFlag(
	const std::variant<ShaderCode, std::string>& variant, ShaderType type,
	std::string_view shader_name
) const {
	if (std::holds_alternative<std::string>(variant)) {
		const auto& name{ std::get<std::string>(variant) };
		path file{ name };
		if (FileExists(file)) {
			return { CompileShaderPath(file, type, shader_name), true };
		} else if (ShaderExists(shader_name, type)) {
			return { GetShaderId(shader_name, type), false };
		} else {
			PTGN_ERROR(name, " is not a valid shader path or loaded ", type, " shader name");
		}
	} else if (std::holds_alternative<ShaderCode>(variant)) {
		const auto& src{ std::get<ShaderCode>(variant) };
		return { CompileShaderSource(src.content, type, shader_name), true };
	} else {
		PTGN_ERROR("Unknown variant type");
	}
}

ShaderId Shaders::CreateProgram(
	const std::variant<ShaderCode, ShaderName>& vertex,
	const std::variant<ShaderCode, ShaderName>& fragment, std::string_view shader_name
) {
	using enum ShaderType;

	auto program{ CreateProgram(shader_name) };

	auto [vertex_id, delete_vert_after] = GetShaderIdWithDeleteFlag(vertex, Vertex, shader_name);
	auto [fragment_id, delete_frag_after] =
		GetShaderIdWithDeleteFlag(fragment, Fragment, shader_name);

	LinkProgram(program, vertex_id, fragment_id);

	if (delete_vert_after && vertex_id) {
		DeleteShaderId(vertex_id, Vertex);
	}

	if (delete_frag_after && fragment_id) {
		DeleteShaderId(fragment_id, Fragment);
	}

	return program;
}

ShaderId Shaders::CreateProgram(
	const std::variant<ShaderCode, path>& source, std::string_view program_name
) {
	auto program{ CreateProgram(program_name) };

	std::string source_string;

	if (std::holds_alternative<path>(source)) {
		const auto& p{ std::get<path>(source) };
		source_string = FileToString(p);
	} else if (std::holds_alternative<ShaderCode>(source)) {
		const auto& src{ std::get<ShaderCode>(source) };
		source_string = src.content;
	} else {
		PTGN_ERROR("Unknown variant type");
	}

	auto srcs{ ParseShaderSourceFile(source_string, program_name) };

	PTGN_ASSERT(
		srcs.size() == 2, "ShaderId file must provide a vertex and fragment type: ", program_name
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

	LinkProgram(program, vertex_id, fragment_id);

	if (vertex_id) {
		DeleteShaderId(vertex_id, ShaderType::Vertex);
	}

	if (fragment_id) {
		DeleteShaderId(fragment_id, ShaderType::Fragment);
	}

	return program;
}

ShaderId Shaders::CreateProgram(std::string_view program_name) {
	ShaderId id{ GLCallReturn(::CreateProgram()) };
#ifdef PTGN_GL_DEBUG_SHADERS
	PTGN_LOG("glCreateProgram() -> id=", id);
#endif

	PTGN_ASSERT(id, "Failed to create shader program");
	cache_.Add(id, ProgramCache{ .program_name = std::string{ program_name } });
	return id;
}

void Shaders::DestroyProgram(ShaderId id) {
	if (!id) {
		return;
	}
	DeleteProgramId(id);
	cache_.Remove(id);
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, V2_float v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(Uniform2f(location, v.x, v.y));
#ifdef PTGN_GL_DEBUG_SHADERS
		PTGN_LOG("glUniform2f(location=", location, ",name=", uniform_name, ",value=", v, ")");
#endif
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, V3_float v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(Uniform3f(location, v.x, v.y, v.z));
#ifdef PTGN_GL_DEBUG_SHADERS
		PTGN_LOG("glUniform3f(location=", location, ",name=", uniform_name, ",value=", v, ")");
#endif
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, V4_float v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(Uniform4f(location, v.x, v.y, v.z, v.w));
#ifdef PTGN_GL_DEBUG_SHADERS
		PTGN_LOG("glUniform4f(location=", location, ",name=", uniform_name, ",value=", v, ")");
#endif
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, const Matrix4& matrix) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		constexpr bool transpose_matrix{ false };
		constexpr int matrix_count{ 1 };
		GLCall(UniformMatrix4fv(location, matrix_count, transpose_matrix, matrix.Data()));
#ifdef PTGN_GL_DEBUG_SHADERS
		PTGN_LOG(
			"glUniformMatrix4fv(location=", location, ",name=", uniform_name, ",value=", matrix, ")"
		);
#endif
	}
}

void Shaders::SetUniform(
	ShaderId id, const char* uniform_name, const std::int32_t* data, std::int32_t count
) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(Uniform1iv(location, count, data));
#ifdef PTGN_GL_DEBUG_SHADERS
		PTGN_LOG(
			"glUniform1iv(location=", location, ",name=", uniform_name, ",count=", count,
			",data=", data, ")"
		);
#endif
	}
}

void Shaders::SetUniform(
	ShaderId id, const char* uniform_name, const float* data, std::int32_t count
) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(Uniform1fv(location, count, data));
#ifdef PTGN_GL_DEBUG_SHADERS
		PTGN_LOG(
			"glUniform1fv(location=", location, ",name=", uniform_name, ",count=", count,
			",data=", data, ")"
		);
#endif
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, const Vector2<std::int32_t>& v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(Uniform2i(location, v.x, v.y));
#ifdef PTGN_GL_DEBUG_SHADERS
		PTGN_LOG("glUniform2i(location=", location, ",name=", uniform_name, ",value=", v, ")");
#endif
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, const Vector3<std::int32_t>& v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(Uniform3i(location, v.x, v.y, v.z));
#ifdef PTGN_GL_DEBUG_SHADERS
		PTGN_LOG("glUniform3i(location=", location, ",name=", uniform_name, ",value=", v, ")");
#endif
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, const Vector4<std::int32_t>& v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(Uniform4i(location, v.x, v.y, v.z, v.w));
#ifdef PTGN_GL_DEBUG_SHADERS
		PTGN_LOG("glUniform4i(location=", location, ",name=", uniform_name, ",value=", v, ")");
#endif
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, float v0) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(Uniform1f(location, v0));
#ifdef PTGN_GL_DEBUG_SHADERS
		PTGN_LOG("glUniform1f(location=", location, ",name=", uniform_name, ",v0=", v0, ")");
#endif
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, float v0, float v1) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(Uniform2f(location, v0, v1));
#ifdef PTGN_GL_DEBUG_SHADERS
		PTGN_LOG(
			"glUniform2f(location=", location, ",name=", uniform_name, ",v0=", v0, ",v1=", v1, ")"
		);
#endif
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, float v0, float v1, float v2) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(Uniform3f(location, v0, v1, v2));
#ifdef PTGN_GL_DEBUG_SHADERS
		PTGN_LOG(
			"glUniform3f(location=", location, ",name=", uniform_name, ",v0=", v0, ",v1=", v1,
			",v2=", v2, ")"
		);
#endif
	}
}

void Shaders::SetUniform(
	ShaderId id, const char* uniform_name, float v0, float v1, float v2, float v3
) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(Uniform4f(location, v0, v1, v2, v3));
#ifdef PTGN_GL_DEBUG_SHADERS
		PTGN_LOG(
			"glUniform4f(location=", location, ",name=", uniform_name, ",v0=", v0, ",v1=", v1,
			",v2=", v2, ",v3=", v3, ")"
		);
#endif
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, std::int32_t v0) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(Uniform1i(location, v0));
#ifdef PTGN_GL_DEBUG_SHADERS
		PTGN_LOG("glUniform1i(location=", location, ",name=", uniform_name, ",v0=", v0, ")");
#endif
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, std::int32_t v0, std::int32_t v1) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(Uniform2i(location, v0, v1));
#ifdef PTGN_GL_DEBUG_SHADERS
		PTGN_LOG(
			"glUniform2i(location=", location, ",name=", uniform_name, ",v0=", v0, ",v1=", v1, ")"
		);
#endif
	}
}

void Shaders::SetUniform(
	ShaderId id, const char* uniform_name, std::int32_t v0, std::int32_t v1, std::int32_t v2
) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(Uniform3i(location, v0, v1, v2));
#ifdef PTGN_GL_DEBUG_SHADERS
		PTGN_LOG(
			"glUniform3i(location=", location, ",name=", uniform_name, ",v0=", v0, ",v1=", v1,
			",v2=", v2, ")"
		);
#endif
	}
}

void Shaders::SetUniform(
	ShaderId id, const char* uniform_name, std::int32_t v0, std::int32_t v1, std::int32_t v2,
	std::int32_t v3
) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(Uniform4i(location, v0, v1, v2, v3));
#ifdef PTGN_GL_DEBUG_SHADERS
		PTGN_LOG(
			"glUniform4i(location=", location, ",name=", uniform_name, ",v0=", v0, ",v1=", v1,
			",v2=", v2, ",v3=", v3, ")"
		);
#endif
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, bool value) {
	SetUniform(id, uniform_name, static_cast<std::int32_t>(value));
}

std::int32_t Shaders::GetUniform(ShaderId id, const char* uniform_name) {
	PTGN_ASSERT(
		gl_.IsBound(id),
		"Cannot get uniform location of shader program which is not currently bound"
	);

	auto& cache{ cache_.Get(id) };

	auto hash{ Hash(uniform_name) };

	if (auto location{ cache.uniform_locations.find(hash) };
		location != cache.uniform_locations.end()) {
		return location->second;
	}

	std::int32_t location{ GLCallReturn(GetUniformLocation(id, uniform_name)) };
#ifdef PTGN_GL_DEBUG_SHADERS
	PTGN_LOG("glGetUniformLocation(id=", id, ",name=", uniform_name, ") -> location=", location);
#endif

	cache.uniform_locations.emplace(hash, location);

	return location;
}

ShaderId Shaders::GetProgram(std::string_view program_name) const {
	auto hash{ Hash(program_name) };
	PTGN_ASSERT(programs_.contains(hash), "No shader program with name '", program_name, "' found");
	return programs_.find(hash)->second;
}

std::ostream& operator<<(std::ostream& os, ShaderType type) {
	switch (type) {
		using enum ShaderType;
		case Vertex:		 return os << "Vertex";
		case Fragment:		 return os << "Fragment";
		case Geometry:		 return os << "Geometry";
		case TessControl:	 return os << "TessControl";
		case TessEvaluation: return os << "TessEvaluation";
		case Compute:		 return os << "Compute";
		default:			 PTGN_ERROR("Unknown ShaderType: ", std::to_underlying(type));
	}
}

} // namespace ptgn::impl::gl