#include "renderer/backend/gl/gl_shader.h"

#include <algorithm>
#include <cmrc/cmrc.hpp>
#include <cstdint>
#include <filesystem>
#include <format>
#include <list>
#include <ostream>
#include <regex>
#include <span>
#include <sstream>
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
#include "core/util/string.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"
#include "serialization/json/json.h"

namespace ptgn::impl::gl {

namespace {

using Header = std::string;

void DeleteShaderId(ShaderId id, [[maybe_unused]] ShaderType type) {
	GLCall(glDeleteShader(id));
}

void DeleteProgramId(ShaderId id) {
	GLCall(glDeleteProgram(id));
}

void LinkProgramId(ShaderId id) {
	GLCall(glLinkProgram(id));
}

std::string_view TrimWhitespace(std::string_view s) {
	std::size_t start{ s.find_first_not_of(" \n\r\t") };
	if (start == std::string::npos) {
		return "";
	}
	std::size_t end{ s.find_last_not_of(" \n\r\t") };
	return s.substr(start, end - start + 1);
}

ShaderType GetShaderType(const std::string& type) {
	if (type == "fragment") {
		return ShaderType::Fragment;
	} else if (type == "vertex") {
		return ShaderType::Vertex;
	}
	PTGN_ERROR("Unknown shader type: ", type);
}

std::string_view GetShaderName(ShaderType type) {
	switch (type) {
		using enum ShaderType;
		case Vertex:		 return "vertex";
		case Fragment:		 return "fragment";
		case Geometry:		 return "geometry";
		case TessControl:	 return "tess_control";
		case TessEvaluation: return "tess_evaluation";
		case Compute:		 return "compute";
		default:			 PTGN_ERROR("Unknown shader type: ", std::to_underlying(type));
	}
}

/// @brief Extract just the content inside R"( ... )"
void TrimRawStringLiteral(std::string& content) {
	const std::string raw_start{ "R\"(" };
	const std::string raw_end{ ")\"" };

	std::size_t start{ content.find(raw_start) };
	std::size_t end{ content.rfind(raw_end) };

	if (start != std::string::npos && end != std::string::npos &&
		end > start + raw_start.length()) {
		content = content.substr(start + raw_start.length(), end - (start + raw_start.length()));
	}
}

std::pair<Header, std::vector<ShaderSpec>> ParseShaderSources(
	const std::string& source, std::string_view name_without_ext
) {
	Header header;
	std::vector<ShaderSpec> sources;

	std::string input{ source };
	TrimRawStringLiteral(input);

	const auto contains_type = [&sources](auto type) {
		return std::ranges::any_of(sources, [type](const ShaderSpec& sts) {
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
		auto pos{ static_cast<std::size_t>(match.position()) };
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
	for (auto i{ 0uz }; i < found_types.size(); ++i) {
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

		PTGN_ASSERT(
			!contains_type(type),
			"GLSL file can only contain one type of shader: ", GetShaderName(type)
		);

		sources.emplace_back(type, ShaderCode{ code }, std::string{ name_without_ext });
	}

	return { header, sources };
}

bool HasOption(std::string_view string, const std::string& option_name) {
	return string.contains("#option " + option_name);
}

void RemoveOption(std::string& source, const std::string& option = "") {
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

std::string InjectShaderPreamble(const std::string& source, [[maybe_unused]] ShaderType type) {
	std::string result{ source };

	std::regex version_regex{ R"(#version\s+(\d+)(?:\s+(\w+))?)" };

	if (std::smatch match; std::regex_search(source, match, version_regex)) {
		// e.g. "330" or "300"
		std::string version_number{ match[1].str() };
		// e.g. "core" or "es"
		std::string version_profile{ match.size() > 2 ? match[2].str() : "" };

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
	std::size_t version_line_end{ result.find('\n') };
	std::size_t insert_pos{ (version_line_end != std::string::npos) ? version_line_end + 1
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

void AddShaderLayout(std::string& source, [[maybe_unused]] ShaderType type) {
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
		R"(^\s*((?:flat|smooth|noperspective)\s+)?(in|out)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*;\r?$)"
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

		// "", "flat", "smooth"
		auto interpolation{ match[1].str() };

		// "in" or "out"
		auto qualifier{ match[2].str() };

#ifdef __EMSCRIPTEN__
		//  Only inject layout for Vertex ShaderId & 'in' variables on WebAssembly or Fragment
		//  shader out variables
		if (!(type == ShaderType::Vertex && qualifier == "in" ||
			  type == ShaderType::Fragment && qualifier == "out")) {
			inject_layout = false;
		}
#endif

		if (inject_layout) {
			std::string variable_type{ match[3].str() }; // (e.g., vec3, int)
			std::string variable_name{ match[4].str() }; // (e.g., a_Position, v_EntityID)

			int location{ (qualifier == "in") ? current_in_location++ : current_out_location++ };

			std::string layout_line{ std::format(
				"layout(location = {}) {}{} {} {};", location, interpolation, qualifier,
				variable_type, variable_name
			) };

			output << layout_line << "\n";
			continue;
		}

		output << line << "\n";
	}

	source = output.str();
}

std::string GenerateTextureColorSwitchBlock(std::size_t max_texture_slots) {
	std::ostringstream oss;
	for (auto i{ 0uz }; i < max_texture_slots; ++i) {
		oss << std::format(
			"\tif (v_TexIndex == {}.0f) {{\n"
			"\t\ttexture_color *= texture(u_Textures[{}], v_TexCoord);\n"
			"\t}}\n",
			i, i
		);
	}
	return oss.str();
}

std::string GenerateTextureSizeSwitchBlock(std::size_t max_texture_slots) {
	std::ostringstream oss;
	for (auto i{ 0uz }; i < max_texture_slots; ++i) {
		oss << std::format(
			"\tif (v_TexIndex == {}.0f) {{\n"
			"\t\ttexture_size = vec2(textureSize(u_Textures[{}], 0));\n"
			"\t}}\n",
			i, i
		);
	}
	return oss.str();
}

std::vector<ShaderSpec> ParseShader(const std::string& source, std::string_view name_without_ext) {
	std::vector<ShaderSpec> output;

	auto [header, sources] = ParseShaderSources(source, name_without_ext);

	// PTGN_LOG("-------- Name ---------");
	// PTGN_LOG(name_without_ext);
	// PTGN_LOG("------- Header ---------");
	// PTGN_LOG(header);

	ShaderOptions global_options;
	global_options.auto_layout = HasOption(header, "auto_layout");

	for (auto i{ 0uz }; i < sources.size(); ++i) {
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

void SubstituteShaderTokens(std::vector<ShaderSpec>& sources, std::size_t max_texture_slots) {
	// This is primarily for the quad shader, which requires a block of if-statements based on
	// how many texture slots there are.

	PTGN_ASSERT(max_texture_slots > 0, "Cannot substitute shader tokens for 0 texture slots");

	std::string color_switch_block{ GenerateTextureColorSwitchBlock(max_texture_slots) };
	std::string size_switch_block{ GenerateTextureSizeSwitchBlock(max_texture_slots) };
	auto slots{ ToString(max_texture_slots) };

	for (auto& sts : sources) {
		sts.code.content = ReplaceAll(sts.code.content, "{MAX_TEXTURE_SLOTS}", slots);
		sts.code.content =
			ReplaceAll(sts.code.content, "{TEXTURE_COLOR_SWITCH_BLOCK}", color_switch_block);
		sts.code.content =
			ReplaceAll(sts.code.content, "{TEXTURE_SIZE_SWITCH_BLOCK}", size_switch_block);
	}
}

json GetShaderManifest(const cmrc::embedded_filesystem& fs) {
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

void Shaders::PopulateShaderCache(const cmrc::embedded_filesystem& filesystem) {
	path subdir{ "" };
	auto dir{ filesystem.iterate_directory(subdir.string()) };

	std::vector<ShaderSpec> sources;

	for (const auto& resource : dir) {
		if (!resource.is_file()) {
			continue;
		}

		path filename{ resource.filename() };

		if (ToLower(filename.extension().string()) != ".glsl") {
			continue;
		}

		auto file{ filesystem.open((subdir / filename).string()) };
		std::string shader_src(file.begin(), file.end());
		std::string name_without_ext{ filename.stem().string() };
		auto srcs{ ParseShader(shader_src, name_without_ext) };
		sources.insert(sources.end(), srcs.begin(), srcs.end());
	}

	SubstituteShaderTokens(sources, max_texture_slots_);
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

	auto fs{ cmrc::shaders::get_filesystem() };

	PopulateShaderCache(fs);

	auto manifest(GetShaderManifest(fs));

	PopulateShadersFromCache(manifest);
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
		PTGN_ASSERT(
			FileExists(path_or_name),
			"Cannot create shader from non-existent file path: ", path_or_name
		);
		return { CompileShaderPath(path_or_name, type, shader_name), true };
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
				static_assert(false, "Incomplete visitor!");
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
				static_assert(false, "Incomplete visitor!");
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

void Shaders::DestroyProgram(ShaderId id) {
	if (!id) {
		return;
	}
	DeleteProgramId(id);
	cache_.Remove(id);
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, const Matrix4& v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		constexpr bool transpose_matrix{ false };
		constexpr int matrix_count{ 1 };
		GLCall(glUniformMatrix4fv(location, matrix_count, transpose_matrix, v.Data()));
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, float v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform1f(location, v));
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, V2_float v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform2f(location, v.x, v.y));
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, V3_float v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform3f(location, v.x, v.y, v.z));
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, V4_float v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform4f(location, v.x, v.y, v.z, v.w));
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, std::span<const float> values) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform1fv(location, static_cast<std::int32_t>(values.size()), values.data()));
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, int v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform1i(location, v));
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, V2_int v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform2i(location, v.x, v.y));
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, V3_int v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform3i(location, v.x, v.y, v.z));
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, V4_int v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform4i(location, v.x, v.y, v.z, v.w));
	}
}

void Shaders::SetUniform(ShaderId id, const char* uniform_name, std::span<const int> v) {
	std::int32_t location{ GetUniform(id, uniform_name) };
	if (location != -1) {
		GLCall(glUniform1iv(location, static_cast<std::int32_t>(v.size()), v.data()));
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
	PTGN_ASSERT(programs_.contains(hash), "No shader program with name '", program_name, "' found");
	return programs_.find(hash)->second;
}

} // namespace ptgn::impl::gl