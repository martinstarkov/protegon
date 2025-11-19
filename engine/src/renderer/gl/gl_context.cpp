#include "renderer/gl/gl_context.h"

#include <cmrc/cmrc.hpp>
#include <ostream>
#include <regex>

#include "SDL_error.h"
#include "SDL_video.h"
#include "core/app/window.h"
#include "core/assert.h"
#include "core/log.h"
#include "core/util/macro.h"
#include "core/util/span.h"
#include "math/hash.h"
#include "renderer/gl/gl.h"

#define PTGN_VSYNC_MODE -1

namespace ptgn::impl::gl {

struct GLVersion {
	GLVersion() {
		int r = SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
		PTGN_ASSERT(!r, SDL_GetError());
		r = SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
		PTGN_ASSERT(!r, SDL_GetError());
	}

	int major{ 0 };
	int minor{ 0 };
};

inline std::ostream& operator<<(std::ostream& os, const GLVersion& v) {
	os << v.major << "." << v.minor;
	return os;
}

// Must be called after SDL and window have been initialized.
static void LoadGLFunctions() {
#ifdef PTGN_PLATFORM_MACOS
	return;
#else

#define GLE(name, caps_name) \
	name =                   \
		reinterpret_cast<PFNGL##caps_name##PROC>(SDL_GL_GetProcAddress(PTGN_STRINGIFY(gl##name)));
	GL_LIST_1
#undef GLE

#ifndef __EMSCRIPTEN__

#define GLE(name, caps_name) \
	name =                   \
		reinterpret_cast<PFNGL##caps_name##PROC>(SDL_GL_GetProcAddress(PTGN_STRINGIFY(gl##name)));
	GL_LIST_2
	GL_LIST_3
#undef GLE

#else

#define GLE(name, caps_name)                                                                       \
	name =                                                                                         \
		reinterpret_cast<PFNGL##caps_name##OESPROC>(SDL_GL_GetProcAddress(PTGN_STRINGIFY(gl##name) \
		));
	GL_LIST_2
#undef GLE
#define GLE(name, caps_name)                                                                       \
	name =                                                                                         \
		reinterpret_cast<PFNGL##caps_name##EXTPROC>(SDL_GL_GetProcAddress(PTGN_STRINGIFY(gl##name) \
		));
	GL_LIST_3
#undef GLE

#endif

	// For debugging which commands were not initialized.
#define GLE(name, caps_name) PTGN_ASSERT(name, "Failed to load ", PTGN_STRINGIFY(name));
	GL_LIST_1
	GL_LIST_2
	GL_LIST_3
#undef GLE

	// Check that each of the loaded gl functions was found.
#define GLE(name, caps_name) name&&
	bool gl_init = GL_LIST_1 GL_LIST_2 GL_LIST_3 true;
#undef GLE
	PTGN_ASSERT(gl_init, "Failed to load OpenGL functions");
	PTGN_INFO("Loaded all OpenGL functions");
#endif
}

// TODO: Move all this code to a shader parsing file.

using Header = std::string;

static std::string TrimWhitespace(const std::string& s) {
	std::size_t start{ s.find_first_not_of(" \n\r\t") };
	if (start == std::string::npos) {
		return "";
	}
	std::size_t end{ s.find_last_not_of(" \n\r\t") };
	return s.substr(start, end - start + 1);
}

static GLenum GetShaderType(const std::string& type) {
	if (type == "fragment") {
		return GL_FRAGMENT_SHADER;
	} else if (type == "vertex") {
		return GL_VERTEX_SHADER;
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

	const auto contains_type = [&sources](GLenum type) {
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

static std::string InjectShaderPreamble(const std::string& source, [[maybe_unused]] GLenum type) {
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

static void AddShaderLayout(std::string& source, [[maybe_unused]] GLenum type) {
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
		if (!(type == GL_VERTEX_SHADER && qualifier == "in")) {
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
		if (global_auto_layout || HasOption(sts.source.source, auto_layout_name)) {
			AddShaderLayout(sts.source.source, sts.type);
		}
		RemoveOption(sts.source.source);
		sts.source.source = InjectShaderPreamble(sts.source.source, sts.type);
		output.emplace_back(sts);
		// PTGN_LOG("------- Source ", i, " (type: ", sts.type, ") -------------");
		// PTGN_LOG(sts.source.source);
	}
	return output;
}

void GLContext::CompileShaders(
	const std::vector<ShaderTypeSource>& sources,
	std::unordered_map<std::size_t, GLuint>& vertex_shaders,
	std::unordered_map<std::size_t, GLuint>& fragment_shaders
) const {
	for (const auto& sts : sources) {
		auto hash{ Hash(sts.name) };
		auto shader_id{ CompileShaderFromSource(sts.type, sts.source.source) };
		switch (sts.type) {
			case GL_FRAGMENT_SHADER:
				PTGN_ASSERT(
					!fragment_shaders.contains(hash), "Cannot add shader to cache twice: ", sts.name
				);
				fragment_shaders.emplace(hash, std::move(shader_id));
				break;
			case GL_VERTEX_SHADER:
				PTGN_ASSERT(
					!vertex_shaders.contains(hash), "Cannot add shader to cache twice: ", sts.name
				);
				vertex_shaders.emplace(hash, std::move(shader_id));
				break;
			default: PTGN_ERROR("Unknown shader type");
		}
	}
}

static void SubstituteShaderTokens(
	std::vector<ShaderTypeSource>& sources, std::size_t max_texture_slots
) {
	// This is primarily for the quad shader, which requires a block of if-statements based on
	// how many texture slots there are.

	PTGN_ASSERT(max_texture_slots > 0, "Cannot substitute shader tokens for 0 texture slots");

	std::string switch_block{ GenerateTextureSwitchBlock(max_texture_slots) };
	auto slots{ std::to_string(max_texture_slots) };

	for (auto& sts : sources) {
		sts.source.source = ReplaceAll(sts.source.source, "{MAX_TEXTURE_SLOTS}", slots);
		sts.source.source = ReplaceAll(sts.source.source, "{TEXTURE_SWITCH_BLOCK}", switch_block);
	}
}

void GLContext::PopulateShaderCache(
	const cmrc::embedded_filesystem& filesystem,
	std::unordered_map<std::size_t, GLuint>& vertex_shaders,
	std::unordered_map<std::size_t, GLuint>& fragment_shaders, std::size_t max_texture_slots
) const {
	const std::string subdir{ "common/" };
	auto dir{ filesystem.iterate_directory(subdir) };

	std::vector<ShaderTypeSource> sources;

	for (auto resource : dir) {
		if (!resource.is_file()) {
			continue;
		}
		auto filename{ resource.filename() };
		auto file{ filesystem.open(subdir + filename) };
		std::string shader_src(file.begin(), file.end());
		std::string name_without_ext{ std::filesystem::path(filename).stem().string() };
		auto srcs{ ParseShader(shader_src, name_without_ext) };
		sources.insert(sources.end(), srcs.begin(), srcs.end());
	}

	SubstituteShaderTokens(sources, max_texture_slots);
	CompileShaders(sources, vertex_shaders, fragment_shaders);
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

GLuint GLContext::CompileShaderFromSource(GLenum type, const std::string& source) const {
	GLuint id{ GLCallReturn(::CreateShader(type)) };

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

		GLCall(DeleteShader(id));

		PTGN_ERROR("Failed to compile ", type, " shader: \n", source, "\n", log);
	}

	return id;
}

void GLContext::PopulateShadersFromCache(const json& manifest) {
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

		PTGN_ASSERT(!shaders_.contains(hash), "Shader names in the manifest must be unique");

		auto shader{ CreateShaderImpl(shader_name) };

		LinkShader(shader, vert_id, frag_id);

		shaders_.emplace(hash, std::move(shader));
	}
}

std::vector<ShaderTypeSource> GLContext::ParseShaderSourceFile(
	const std::string& source, const std::string& name, std::size_t max_texture_slots
) const {
	auto srcs{ ParseShader(source, name) };
	SubstituteShaderTokens(srcs, max_texture_slots);
	return srcs;
}

GLuint GLContext::CompileShaderSource(
	const std::string& source, GLenum type, const std::string& name, std::size_t max_texture_slots
) const {
	auto srcs{ ParseShaderSourceFile(source, name, max_texture_slots) };
	PTGN_ASSERT(srcs.size() == 1, "Wrong constructor for a multi-source shader file");
	const auto& front{ srcs.front() };
	PTGN_ASSERT(front.type == type, "Shader type mismatch");
	return CompileShaderFromSource(type, front.source.source);
}

GLuint GLContext::CompileShaderPath(
	const path& shader_path, GLenum type, const std::string& name, std::size_t max_texture_slots
) const {
	PTGN_ASSERT(
		FileExists(shader_path),
		"Cannot create shader from nonexistent shader path: ", shader_path.string()
	);
	auto source{ FileToString(shader_path) };
	return CompileShaderSource(source, type, name, max_texture_slots);
}

void GLContext::LinkShader(GLuint id, GLuint vertex, GLuint fragment) {
	shader_cache_.Get(id).uniform_locations.Clear();

	PTGN_ASSERT(vertex);
	PTGN_ASSERT(fragment);

	GLCall(AttachShader(id, vertex));
	GLCall(AttachShader(id, fragment));
	GLCall(LinkProgram(id));

	// Check for shader link errors.
	std::int32_t linked{ GL_FALSE };
	GLCall(GetProgramiv(id, GL_LINK_STATUS, &linked));

	if (linked == GL_FALSE) {
		std::int32_t length{ 0 };
		GLCall(GetProgramiv(id, GL_INFO_LOG_LENGTH, &length));
		std::string log;
		log.resize(static_cast<std::size_t>(length));
		GLCall(GetProgramInfoLog(id, length, &length, &log[0]));

		GLCall(DeleteProgram(id));

		GLCall(DeleteShader(vertex));
		GLCall(DeleteShader(fragment));

		PTGN_ERROR(
			"Failed to link shaders to program:\nVertex : ", vertex, "\nFragment : ", fragment,
			"\n ", log
		);
	}

	GLCall(ValidateProgram(id));
}

void GLContext::CompileShader(
	GLuint id, const std::string& vertex_source, const std::string& fragment_source
) const {
	// TODO: Ensure shader cache is cleared if it exists.

	GLuint vertex{ CompileShaderFromSource(GL_VERTEX_SHADER, vertex_source) };
	GLuint fragment{ CompileShaderFromSource(GL_FRAGMENT_SHADER, fragment_source) };

	if (vertex && fragment) {
		GLCall(AttachShader(id, vertex));
		GLCall(AttachShader(id, fragment));
		GLCall(LinkProgram(id));

		// Check for shader link errors.
		std::int32_t linked{ GL_FALSE };
		GLCall(GetProgramiv(id, GL_LINK_STATUS, &linked));

		if (linked == GL_FALSE) {
			std::int32_t length{ 0 };
			GLCall(GetProgramiv(id, GL_INFO_LOG_LENGTH, &length));
			std::string log;
			log.resize(static_cast<std::size_t>(length));
			GLCall(GetProgramInfoLog(id, length, &length, &log[0]));

			GLCall(DeleteProgram(id));

			GLCall(DeleteShader(vertex));
			GLCall(DeleteShader(fragment));

			PTGN_ERROR(
				"Failed to link shaders to program: \n", vertex_source, "\n", fragment_source, "\n",
				log
			);
		}

		GLCall(ValidateProgram(id));
	}

	if (vertex) {
		GLCall(DeleteShader(vertex));
	}

	if (fragment) {
		GLCall(DeleteShader(fragment));
	}
}

GLContext::GLContext(Window& window) {
	if (context_ != nullptr) {
		int result = SDL_GL_MakeCurrent(window, context_);
		PTGN_ASSERT(!result, SDL_GetError());
		return;
	}

	context_ = SDL_GL_CreateContext(window);
	PTGN_ASSERT(context_, SDL_GetError());

	GLVersion gl_version;

	PTGN_INFO("Initialized OpenGL version: ", gl_version);
	PTGN_INFO("Created OpenGL context");

	// From: https://nullprogram.com/blog/2023/01/08/
	// Set a non-zero SDL_GL_SetSwapInterval so that SDL_GL_SwapWindow synchronizes.
	if (!SDL_GL_SetSwapInterval(PTGN_VSYNC_MODE)) {
		// If no adaptive VSYNC available, fallback to VSYNC.
		SDL_GL_SetSwapInterval(1);
	}

	LoadGLFunctions();

	// PTGN_LOG("OpenGL Build: ", GLCall(glGetString(GL_VERSION)));

	auto max_texture_slots{ GetInteger<GLuint>(GL_MAX_TEXTURE_IMAGE_UNITS) };
	PTGN_ASSERT(max_texture_slots > 0);
	bound_.texture_units.resize(max_texture_slots, {});

	auto fs{ cmrc::shader::get_filesystem() };

	PopulateShaderCache(fs, vertex_shaders_, fragment_shaders_, max_texture_slots);

	auto manifest = GetShaderManifest(fs);

	PopulateShadersFromCache(manifest);
}

GLContext::~GLContext() {
	const auto delete_shaders = [](const auto& container) {
		for (const auto& [hash, id] : container) {
			if (id) {
				GLCall(DeleteShader(id));
			}
		}
	};

	// Delete cached vertex and fragment shaders.
	delete_shaders(vertex_shaders_);
	delete_shaders(fragment_shaders_);

	if (context_) {
		SDL_GL_DeleteContext(context_);
		context_ = nullptr;
		PTGN_INFO("Destroyed OpenGL context");
	}
}

} // namespace ptgn::impl::gl
